#pragma once

#include "plugin.h"

#include <atomic>
#include <string_view>

/// <summary>
/// Stores the function pointers to the functions imported from the VarjoLib library.
/// </summary>
struct VarjoApi {

    decltype(&varjo_GetSwapChainImage) GetSwapChainImage = nullptr;
    decltype(&varjo_AcquireSwapChainImage) AcquireSwapChainImage = nullptr;
    decltype(&varjo_ReleaseSwapChainImage) ReleaseSwapChainImage = nullptr;
    decltype(&varjo_FreeSwapChain) FreeSwapChain = nullptr;
    decltype(&varjo_GetError) GetError = nullptr;
    decltype(&varjo_GetErrorDesc) GetErrorDesc = nullptr;

    decltype(&varjo_D3D11CreateSwapChain) D3D11CreateSwapChain = nullptr;
    decltype(&varjo_ToD3D11Texture) ToD3D11Texture = nullptr;

    decltype(&varjo_D3D12CreateSwapChain) D3D12CreateSwapChain = nullptr;
    decltype(&varjo_ToD3D12Texture) ToD3D12Texture = nullptr;

    /// <summary>
    /// Resolves the function pointers from the library module.
    /// </summary>
    /// <param name="varjoLib">The VarjoLib library module.</param>
    /// <returns>`true`, if the resolution was successful and `false` otherwise.</returns>
    bool Resolve(HMODULE varjoLib);

    /// <summary>
    /// Checks if the function pointers from the library are resolved.
    /// </summary>
    /// <returns>`true`, if the functions are resolved and `false` otherwise.</returns>
    bool IsResolved() const {
        return m_resolved.load(std::memory_order_acquire);
    }

    /// <summary>
    /// Checks if the VarjoLib instance supports rendering to D3D11.
    /// </summary>
    /// <returns>`true` if the VarjoLib instance supports rendering to D3D11 and `false` otherwise.</returns>
    bool SupportsD3D11() const {
        return IsResolved() && D3D11CreateSwapChain && ToD3D11Texture;
    }

    /// <summary>
    /// Checks if the VarjoLib instance supports rendering to D3D12.
    /// </summary>
    /// <returns>`true` if the VarjoLib instance supports rendering to D3D12 and `false` otherwise.</returns>
    bool SupportsD3D12() const {
        return IsResolved() && D3D12CreateSwapChain && ToD3D12Texture;
    }

private:
    std::atomic<bool> m_resolved{ false };
    HMODULE m_module = nullptr;
};

extern VarjoApi g_varjo;

/// <summary>
/// Logs pending errors from <paramref name="session" />.
/// </summary>
/// <param name="session">The session from which to obtain the error.</param>
/// <param name="context">A string describing the context under which the error occurred.</param>
void LogVarjoError(varjo_Session* session, std::string_view context);
