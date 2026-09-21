#pragma once

#include "plugin.h"
#include "layer_types.h"

#include <wrl/client.h>

#include <array>
#include <cstdint>
#include <memory>
#include <span>

constexpr int32_t kSwapChainLength = 3;

enum class TextureFormat : int32_t {
    Unknown = 0,
    R8G8B8A8Srgb,
    D32Float,
    D24UnormS8Uint,
    D32FloatS8Uint
};

constexpr varjo_TextureFormat ToVarjoFormat(TextureFormat format) {
    switch (format) {
    case TextureFormat::R8G8B8A8Srgb: return varjo_TextureFormat_R8G8B8A8_SRGB;
    case TextureFormat::D32Float: return varjo_DepthTextureFormat_D32_FLOAT;
    case TextureFormat::D24UnormS8Uint: return varjo_DepthTextureFormat_D24_UNORM_S8_UINT;
    case TextureFormat::D32FloatS8Uint: return varjo_DepthTextureFormat_D32_FLOAT_S8_UINT;
    default: return 0;
    }
}

struct TextureDesc {
    int32_t width = 0;
    int32_t height = 0;
    TextureFormat format = TextureFormat::Unknown;

    bool operator==(const TextureDesc&) const = default;
};

struct SourceTexture {
    Microsoft::WRL::ComPtr<IUnknown> resource;
    TextureDesc desc;
};

struct SwapChain {
    varjo_SwapChain* handle = nullptr;
    TextureDesc desc;
    std::array<IUnknown*, kSwapChainLength> images{};
};

struct CopyRequest {
    IUnknown* source;
    IUnknown* destination;
    TextureUsage usage;
};

struct CopySource {
    IUnknown* resource;
    TextureUsage usage;
};

class GraphicsBackend {
public:
    virtual ~GraphicsBackend() = default;

    virtual UnityGfxRenderer Renderer() const = 0;
    virtual bool HasVarjoSupport() const = 0;
    virtual VarjoLayers_Result Register(void* nativeTexture, TextureUsage usage, SourceTexture& texture) const = 0;
    virtual bool CreateSwapChain(varjo_Session* session, const TextureDesc& desc, SwapChain& swapChain) = 0;
    virtual bool Copy(std::span<const CopyRequest> requests) = 0;
    virtual void WaitForIdle() { }

    virtual bool RequiresPreparation() const {
        return false;
    }

    virtual void PrepareCopySources(std::span<const CopySource> /*sources*/) { }
};

std::shared_ptr<GraphicsBackend> CreateD3D11Backend(IUnityInterfaces& interfaces);
std::shared_ptr<GraphicsBackend> CreateD3D12Backend(IUnityInterfaces& interfaces);
