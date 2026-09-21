#include "d3d12_backend.h"
#include "dxgi_formats.h"
#include "log.h"
#include "varjo_api.h"

#include <optional>
#include <utility>

using Microsoft::WRL::ComPtr;

namespace {

    constexpr D3D12_RESOURCE_STATES SwapChainImageState(TextureUsage usage) {
        return usage == TextureUsage::Depth ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_RENDER_TARGET;
    }

    D3D12_RESOURCE_BARRIER Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = resource;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = after;

        return barrier;
    }

    D3D12_TEXTURE_COPY_LOCATION FirstSubresource(ID3D12Resource* resource) {
        D3D12_TEXTURE_COPY_LOCATION location{};
        location.pResource = resource;
        location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        location.SubresourceIndex = 0;

        return location;
    }

    template <typename Interface>
    std::optional<D3D12Backend::UnityD3D12> Adapt(Interface* unityD3D12) {
        if (!unityD3D12)
            return std::nullopt;

        return D3D12Backend::UnityD3D12{
            .GetDevice = unityD3D12->GetDevice,
            .GetCommandQueue = unityD3D12->GetCommandQueue,
            .ExecuteCommandList = unityD3D12->ExecuteCommandList,
        };
    }

}

std::shared_ptr<GraphicsBackend> CreateD3D12Backend(IUnityInterfaces& interfaces) {
    auto unity = Adapt(interfaces.Get<IUnityGraphicsD3D12v7>());

    if (!unity)
        unity = Adapt(interfaces.Get<IUnityGraphicsD3D12v6>());

    if (!unity)
        unity = Adapt(interfaces.Get<IUnityGraphicsD3D12v5>());

    if (!unity)
        unity = Adapt(interfaces.Get<IUnityGraphicsD3D12v4>());

    if (!unity) {
        LogEvent("D3D12: Unity provides no supported IUnityGraphicsD3D12 interface (v4 or later)");
        return nullptr;
    }

    ComPtr<ID3D12Device> device = unity->GetDevice();
    ComPtr<ID3D12CommandQueue> queue = unity->GetCommandQueue();

    if (!device || !queue)
        return nullptr;

    auto backend = std::make_shared<D3D12Backend>(*unity, std::move(device), std::move(queue));

    if (!backend->Initialize())
        return nullptr;

    return backend;
}

D3D12Backend::D3D12Backend(const UnityD3D12& unity, ComPtr<ID3D12Device> device, ComPtr<ID3D12CommandQueue> queue) :
    m_unity(unity),
    m_device(std::move(device)),
    m_queue(std::move(queue))
{
}

D3D12Backend::~D3D12Backend() {
    if (m_fence)
        WaitForIdle();

    if (m_fenceEvent)
        CloseHandle(m_fenceEvent);
}

bool D3D12Backend::Initialize() {
    for (auto& allocator : m_allocators) {
        if (FAILED(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)))) {
            LogEvent("D3D12: creating a command allocator failed");
            return false;
        }
    }

    if (FAILED(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_allocators[0].Get(), nullptr, IID_PPV_ARGS(&m_commandList))) ||
        FAILED(m_commandList->Close())) {
        LogEvent("D3D12: creating the copy command list failed");
        return false;
    }

    if (FAILED(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_emptyAllocator))) ||
        FAILED(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_emptyAllocator.Get(), nullptr, IID_PPV_ARGS(&m_emptyCommandList))) ||
        FAILED(m_emptyCommandList->Close())) {
        LogEvent("D3D12: creating the preparation command list failed");
        return false;
    }

    if (FAILED(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)))) {
        LogEvent("D3D12: creating the copy fence failed");
        return false;
    }

    m_fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);

    if (!m_fenceEvent) {
        LogEvent("D3D12: creating the fence event failed");
        return false;
    }

    m_commandList->SetName(L"VarjoXRLayersSupport copy");
    m_emptyCommandList->SetName(L"VarjoXRLayersSupport prepare");
    m_fence->SetName(L"VarjoXRLayersSupport copy fence");

    m_barriers.reserve(64);
    m_states.reserve(8);

    return true;
}

bool D3D12Backend::HasVarjoSupport() const {
    return g_varjo.SupportsD3D12();
}

VarjoLayers_Result D3D12Backend::Register(void* nativeTexture, TextureUsage usage, SourceTexture& texture) const {
    ComPtr<ID3D12Resource> resource;

    if (FAILED(static_cast<IUnknown*>(nativeTexture)->QueryInterface(IID_PPV_ARGS(&resource))))
        return VarjoLayers_Result::WrongGraphicsApi;

    ComPtr<ID3D12Device> device;

    if (FAILED(resource->GetDevice(IID_PPV_ARGS(&device))) || device != m_device)
        return VarjoLayers_Result::WrongGraphicsApi;

    const D3D12_RESOURCE_DESC desc = resource->GetDesc();

    if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || desc.DepthOrArraySize != 1 || desc.SampleDesc.Count != 1)
        return VarjoLayers_Result::UnsupportedLayout;

    const TextureFormat format = ToTextureFormat(desc.Format, usage);

    if (format == TextureFormat::Unknown)
        return VarjoLayers_Result::UnsupportedFormat;

    texture.resource = resource;
    texture.desc = {
        .width = static_cast<int32_t>(desc.Width),
        .height = static_cast<int32_t>(desc.Height),
        .format = format
    };

    return VarjoLayers_Result::Ok;
}

bool D3D12Backend::CreateSwapChain(varjo_Session* session, const TextureDesc& desc, SwapChain& swapChain) {
    varjo_SwapChainConfig2 config = {
        .textureFormat = ToVarjoFormat(desc.format),
        .numberOfTextures = kSwapChainLength,
        .textureWidth = desc.width,
        .textureHeight = desc.height,
        .textureArraySize = 1,
    };
    
    varjo_SwapChain* handle = g_varjo.D3D12CreateSwapChain(session, m_queue.Get(), &config);

    if (!handle) {
        LogVarjoError(session, "varjo_D3D12CreateSwapChain failed");
        return false;
    }

    SwapChain result { .handle = handle, .desc = desc };

    for (int32_t index = 0; index < kSwapChainLength; ++index) {
        result.images[index] = g_varjo.ToD3D12Texture(g_varjo.GetSwapChainImage(handle, index));

        if (!result.images[index]) {
            LogEvent("swap chain image %d could not be converted to a D3D12 texture", index);
            g_varjo.FreeSwapChain(handle);
            return false;
        }
    }

    swapChain = result;

    return true;
}

void D3D12Backend::PrepareCopySources(std::span<const CopySource> sources) {
    if (sources.empty())
        return;

    m_states.clear();

    for (const CopySource& source : sources) {
        m_states.push_back(UnityGraphicsD3D12ResourceState{
            .resource = static_cast<ID3D12Resource*>(source.resource),
            .expected = D3D12_RESOURCE_STATE_COPY_SOURCE,
            .current = D3D12_RESOURCE_STATE_COPY_SOURCE,
        });
    }

    m_unity.ExecuteCommandList(m_emptyCommandList.Get(), static_cast<int>(m_states.size()), m_states.data());
}

bool D3D12Backend::Copy(std::span<const CopyRequest> requests) {
    if (requests.empty())
        return true;

    const auto slot = static_cast<size_t>(m_frame++ % kFramesInFlight);
    WaitForFence(m_allocatorFenceValues[slot]);

    ID3D12CommandAllocator* allocator = m_allocators[slot].Get();

    if (FAILED(allocator->Reset()) || FAILED(m_commandList->Reset(allocator, nullptr))) {
        LogEvent("D3D12: resetting the copy command list failed");
        return false;
    }

    m_barriers.clear();

    for (const CopyRequest& request : requests)
        m_barriers.push_back(Transition(static_cast<ID3D12Resource*>(request.destination), SwapChainImageState(request.usage), D3D12_RESOURCE_STATE_COPY_DEST));

    m_commandList->ResourceBarrier(static_cast<UINT>(m_barriers.size()), m_barriers.data());

    for (const CopyRequest& request : requests) {
        const D3D12_TEXTURE_COPY_LOCATION destination = FirstSubresource(static_cast<ID3D12Resource*>(request.destination));
        const D3D12_TEXTURE_COPY_LOCATION source = FirstSubresource(static_cast<ID3D12Resource*>(request.source));
        m_commandList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    }

    for (D3D12_RESOURCE_BARRIER& barrier : m_barriers)
        std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);

    m_commandList->ResourceBarrier(static_cast<UINT>(m_barriers.size()), m_barriers.data());

    if (FAILED(m_commandList->Close())) {
        LogEvent("D3D12: closing the copy command list failed");
        return false;
    }

    ID3D12CommandList* commandLists[] = { m_commandList.Get() };
    m_queue->ExecuteCommandLists(1, commandLists);

    if (FAILED(m_queue->Signal(m_fence.Get(), ++m_fenceValue))) {
        LogEvent("D3D12: signaling the copy fence failed");
        return false;
    }

    m_allocatorFenceValues[slot] = m_fenceValue;

    return true;
}

void D3D12Backend::WaitForIdle() {
    WaitForFence(m_fenceValue);
}

void D3D12Backend::WaitForFence(UINT64 value) {
    if (m_fence->GetCompletedValue() >= value)
        return;

    if (SUCCEEDED(m_fence->SetEventOnCompletion(value, m_fenceEvent)))
        WaitForSingleObject(m_fenceEvent, INFINITE);
}
