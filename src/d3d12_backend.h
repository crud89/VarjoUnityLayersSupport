#pragma once

#include "graphics_backend.h"

#include <array>
#include <vector>

class D3D12Backend final : public GraphicsBackend {
public:
    struct UnityD3D12 {
        ID3D12Device* (UNITY_INTERFACE_API* GetDevice)();
        ID3D12CommandQueue* (UNITY_INTERFACE_API* GetCommandQueue)();
        UINT64 (UNITY_INTERFACE_API* ExecuteCommandList)(ID3D12GraphicsCommandList* commandList, int stateCount, UnityGraphicsD3D12ResourceState* states);
    };

    D3D12Backend(const UnityD3D12& unity, Microsoft::WRL::ComPtr<ID3D12Device> device, Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue);
    ~D3D12Backend() override;

    bool Initialize();

    UnityGfxRenderer Renderer() const override {
        return kUnityGfxRendererD3D12;
    }

    bool HasVarjoSupport() const override;
    VarjoLayers_Result Register(void* nativeTexture, TextureUsage usage, SourceTexture& texture) const override;
    bool CreateSwapChain(varjo_Session* session, const TextureDesc& desc, SwapChain& swapChain) override;
    bool Copy(std::span<const CopyRequest> requests) override;
    void WaitForIdle() override;

    bool RequiresPreparation() const override {
        return true;
    }

    void PrepareCopySources(std::span<const CopySource> sources) override;

private:
    static constexpr int32_t kFramesInFlight = 3;

    void WaitForFence(UINT64 value);

    UnityD3D12 m_unity;
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_queue;

    std::array<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>, kFramesInFlight> m_allocators;
    std::array<UINT64, kFramesInFlight> m_allocatorFenceValues{};
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue = 0;
    HANDLE m_fenceEvent = nullptr;
    uint64_t m_frame = 0;

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_emptyAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_emptyCommandList;

    std::vector<D3D12_RESOURCE_BARRIER> m_barriers;
    std::vector<UnityGraphicsD3D12ResourceState> m_states;
};
