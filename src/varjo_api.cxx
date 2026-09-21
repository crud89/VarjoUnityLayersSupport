#include "varjo_api.h"
#include "log.h"

#include <format>

VarjoApi g_varjo;

namespace {

    /// <summary>
    /// Resolves a function from a module.
    /// </summary>
    /// <typeparam name="Fn">The type of the function to resolve.</typeparam>
    /// <param name="module">The module from which to resolve the function.</param>
    /// <param name="name">The name of the function.</param>
    /// <param name="target">A reference to the function pointer.</param>
    /// <returns>`true` if the function was successfully resolved and `false` otherwise.</returns>
    template <typename Fn>
    bool ResolveSymbol(HMODULE module, const char* name, Fn& target) {
        target = reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(module, name)));

        if (!target)
            LogEvent("VarjoLib.dll does not export %s", name);

        return target != nullptr;
    }

}

bool VarjoApi::Resolve(HMODULE varjoLib) {
    if (IsResolved() && varjoLib == m_module)
        return true;

    if (!varjoLib)
        return false;

    // Load all functions that are required.
    bool requiredFunctions =
        ResolveSymbol(varjoLib, "varjo_GetSwapChainImage", GetSwapChainImage) &&
        ResolveSymbol(varjoLib, "varjo_AcquireSwapChainImage", AcquireSwapChainImage) &&
        ResolveSymbol(varjoLib, "varjo_ReleaseSwapChainImage", ReleaseSwapChainImage) &&
        ResolveSymbol(varjoLib, "varjo_FreeSwapChain", FreeSwapChain) &&
        ResolveSymbol(varjoLib, "varjo_GetError", GetError) &&
        ResolveSymbol(varjoLib, "varjo_GetErrorDesc", GetErrorDesc);

    // Load optional functions (may be missing, depending on backend availability).
    ResolveSymbol(varjoLib, "varjo_D3D11CreateSwapChain", D3D11CreateSwapChain);
    ResolveSymbol(varjoLib, "varjo_ToD3D11Texture", ToD3D11Texture);

    ResolveSymbol(varjoLib, "varjo_D3D12CreateSwapChain", D3D12CreateSwapChain);
    ResolveSymbol(varjoLib, "varjo_ToD3D12Texture", ToD3D12Texture);

    // Store the resolution state and module.
    m_module = varjoLib;
    m_resolved.store(requiredFunctions, std::memory_order_release);

    return requiredFunctions;
}

void LogVarjoError(varjo_Session* session, std::string_view context) {
    auto error = g_varjo.GetError(session);
    auto description = g_varjo.GetErrorDesc(error);

    auto msg = std::format("{}: {} ({})", context, description ? description : "no description", static_cast<long long>(error));
    LogEvent("%s", msg.c_str());
}
