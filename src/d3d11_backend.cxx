#include "d3d11_backend.h"
#include "dxgi_formats.h"
#include "log.h"
#include "varjo_api.h"

using Microsoft::WRL::ComPtr;

std::shared_ptr<GraphicsBackend> CreateD3D11Backend(IUnityInterfaces& interfaces) {
    auto* unityD3D11 = interfaces.Get<IUnityGraphicsD3D11>();

    if (!unityD3D11)
        return nullptr;

    ComPtr<ID3D11Device> device = unityD3D11->GetDevice();

    if (!device)
        return nullptr;

    return std::make_shared<D3D11Backend>(std::move(device));
}

D3D11Backend::D3D11Backend(ComPtr<ID3D11Device> device) :
    m_device(std::move(device)) 
{
    m_device->GetImmediateContext(&m_context);
}

bool D3D11Backend::HasVarjoSupport() const {
    return g_varjo.SupportsD3D11();
}

VarjoLayers_Result D3D11Backend::Register(void* nativeTexture, TextureUsage usage, SourceTexture& texture) const {
    ComPtr<ID3D11Texture2D> texture2D;

    if (FAILED(static_cast<IUnknown*>(nativeTexture)->QueryInterface(IID_PPV_ARGS(&texture2D))))
        return VarjoLayers_Result::WrongGraphicsApi;

    ComPtr<ID3D11Device> device;
    texture2D->GetDevice(&device);

    if (device != m_device)
        return VarjoLayers_Result::WrongGraphicsApi;

    D3D11_TEXTURE2D_DESC desc{};
    texture2D->GetDesc(&desc);

    const TextureFormat format = ToTextureFormat(desc.Format, usage);

    if (format == TextureFormat::Unknown)
        return VarjoLayers_Result::UnsupportedFormat;

    if (desc.SampleDesc.Count != 1 || desc.ArraySize != 1)
        return VarjoLayers_Result::UnsupportedLayout;

    texture.resource = texture2D;
    texture.desc = { 
        .width = static_cast<int32_t>(desc.Width),
        .height = static_cast<int32_t>(desc.Height),
        .format = format
    };

    return VarjoLayers_Result::Ok;
}

bool D3D11Backend::CreateSwapChain(varjo_Session* session, const TextureDesc& desc, SwapChain& swapChain) {
    varjo_SwapChainConfig2 config = {
        .textureFormat = ToVarjoFormat(desc.format),
        .numberOfTextures = kSwapChainLength,
        .textureWidth = desc.width,
        .textureHeight = desc.height,
        .textureArraySize = 1,
    };

    varjo_SwapChain* handle = g_varjo.D3D11CreateSwapChain(session, m_device.Get(), &config);

    if (!handle) {
        LogVarjoError(session, "varjo_D3D11CreateSwapChain failed");
        return false;
    }

    SwapChain result { .handle = handle, .desc = desc };

    for (int32_t index = 0; index < kSwapChainLength; ++index) {
        result.images[index] = g_varjo.ToD3D11Texture(g_varjo.GetSwapChainImage(handle, index));

        if (!result.images[index]) {
            LogEvent("swap chain image %d could not be converted to a D3D11 texture", index);
            g_varjo.FreeSwapChain(handle);
            return false;
        }
    }

    swapChain = result;

    return true;
}

bool D3D11Backend::Copy(std::span<const CopyRequest> requests) {
    for (const CopyRequest& request : requests)
        m_context->CopySubresourceRegion(static_cast<ID3D11Texture2D*>(request.destination), 0, 0, 0, 0,
            static_cast<ID3D11Texture2D*>(request.source), 0, nullptr);

    return true;
}
