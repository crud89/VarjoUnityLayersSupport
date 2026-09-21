#pragma once

#include "graphics_backend.h"

inline TextureFormat ToTextureFormat(DXGI_FORMAT format, TextureUsage usage) {
    if (usage == TextureUsage::Color) {
        switch (format) {
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            return TextureFormat::R8G8B8A8Srgb;
        default:
            return TextureFormat::Unknown;
        }
    }

    switch (format) {
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
        return TextureFormat::D32Float;
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
        return TextureFormat::D24UnormS8Uint;
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        return TextureFormat::D32FloatS8Uint;
    default:
        return TextureFormat::Unknown;
    }
}
