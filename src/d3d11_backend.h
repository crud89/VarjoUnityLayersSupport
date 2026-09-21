#pragma once

#include "graphics_backend.h"

class D3D11Backend final : public GraphicsBackend {
public:
    explicit D3D11Backend(Microsoft::WRL::ComPtr<ID3D11Device> device);

    UnityGfxRenderer Renderer() const override {
        return kUnityGfxRendererD3D11;
    }

    bool HasVarjoSupport() const override;
    VarjoLayers_Result Register(void* nativeTexture, TextureUsage usage, SourceTexture& texture) const override;
    bool CreateSwapChain(varjo_Session* session, const TextureDesc& desc, SwapChain& swapChain) override;
    bool Copy(std::span<const CopyRequest> requests) override;

private:
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
};
