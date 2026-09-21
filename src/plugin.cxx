#include "plugin.h"

#include "hook.h"
#include "layer_stack.h"
#include "log.h"
#include "unity_interface.h"
#include "varjo_api.h"

#include <atomic>
#include <cstdint>

namespace {

    constexpr wchar_t kPluginModule[] = L"VarjoUnityXR.dll";
    constexpr wchar_t kVarjoLibModule[] = L"VarjoLib.dll";
    constexpr char kVarjoLibImport[] = "VarjoLib.dll";

    enum InstallStatus : uint32_t {
        kModuleFound = 1u << 0,
        kVarjoApiResolved = 1u << 1,
        kEndFramePatched = 1u << 2,
        kShutdownPatched = 1u << 3,
        kVarjoLibDelayLoaded = 1u << 4,
        kUnityPluginLoaded = 1u << 5,
        kGraphicsBackendReady = 1u << 6
    };

    using EndFrameWithLayersFn = decltype(&varjo_EndFrameWithLayers);
    using SessionShutDownFn = decltype(&varjo_SessionShutDown);

    std::atomic<EndFrameWithLayersFn> g_originalEndFrame{nullptr};
    std::atomic<SessionShutDownFn> g_originalShutDown{nullptr};

    void HookEndFrameWithLayers(varjo_Session* session, varjo_SubmitInfoLayers* submitInfo) noexcept {
        g_layers.Submit(session, submitInfo, g_originalEndFrame.load(std::memory_order_acquire));
    }

    void HookSessionShutDown(varjo_Session* session) noexcept {
        g_layers.OnSessionShutDown();
        g_originalShutDown.load(std::memory_order_acquire)(session);
    }

}

VARJO_XR_LAYERS_SUPPORT_API void VarjoLayers_SetLogPath(const wchar_t* path) { 
    LogSetPath(path); 
}

VARJO_XR_LAYERS_SUPPORT_API uint32_t VarjoLayers_Install() {
    uint32_t status = 0;

    g_layers.Reset();

    HMODULE plugin = GetModuleHandleW(kPluginModule);

    if (!plugin) {
        LogEvent("install failed: VarjoUnityXR.dll is not loaded");
        return status;
    }

    status |= kModuleFound;

    if (IsUnityPluginLoaded())
        status |= kUnityPluginLoaded;

    if (g_layers.HasBackend())
        status |= kGraphicsBackendReady;

    if (IsDelayLoaded(plugin, kVarjoLibImport)) 
        status |= kVarjoLibDelayLoaded;

    if (g_varjo.Resolve(GetModuleHandleW(kVarjoLibModule))) 
        status |= kVarjoApiResolved;

    if (status & kVarjoApiResolved) {
        if (void** slot = FindIatSlot(plugin, kVarjoLibImport, "varjo_SessionShutDown"); slot && PatchIatSlot(slot, &HookSessionShutDown, g_originalShutDown))
            status |= kShutdownPatched;

        if (status & kShutdownPatched) {
            if (void** slot = FindIatSlot(plugin, kVarjoLibImport, "varjo_EndFrameWithLayers"); slot && PatchIatSlot(slot, &HookEndFrameWithLayers, g_originalEndFrame))
                status |= kEndFramePatched;
        }
    }

    LogEvent("install status 0x%02X", status);

    return status;
}

void VarjoLayers_SetEnabled(bool enabled) {
    g_layers.SetEnabled(enabled);
}

VarjoLayers_Result VarjoLayers_SetApplicationBaseLayer(const VarjoApplicationBaseLayerDesc* desc) {
    return g_layers.SetApplicationLayer(desc);
}

VarjoLayers_Handle VarjoLayers_CreateLayer(const VarjoLayerDesc* desc) {
    return g_layers.CreateLayer(desc);
}

VarjoLayers_Result VarjoLayers_UpdateLayer(VarjoLayers_Handle layer, const VarjoLayerDesc* desc) {
    return g_layers.UpdateLayer(layer, desc);
}

VarjoLayers_Result VarjoLayers_DestroyLayer(VarjoLayers_Handle layer) {
    return g_layers.DestroyLayer(layer);
}

VarjoLayers_Result VarjoLayers_SetLayerTexture(VarjoLayers_Handle layer, int32_t view, TextureUsage usage, void* nativeTexture) {
    return g_layers.SetLayerTexture(layer, view, usage, nativeTexture);
}

VarjoLayers_Result VarjoLayers_ClearLayerTextures(VarjoLayers_Handle layer) {
    return g_layers.ClearLayerTextures(layer);
}

uint32_t VarjoLayers_StageLayerViews(VarjoLayers_Handle layer, const VarjoLayerView* views, int32_t viewCount) {
    return g_layers.PushLayerViewMatrices(layer, views, viewCount);
}

int32_t VarjoLayers_GetApplicationBaseLayerViews(VarjoLayerView* views, int32_t capacity) {
    return g_layers.GetApplicationBaseLayerViews(views, capacity);
}

void VarjoLayers_GetFrameStats(VarjoFrameStats* stats) {
    if (stats)
        *stats = g_layers.GetFrameStats();
}

VarjoLayers_Result VarjoLayers_GetLayerStats(VarjoLayers_Handle layer, VarjoLayerStats* stats) {
    return g_layers.GetLayerStats(layer, stats);
}

void VarjoLayers_ResetStats() {
    g_layers.ResetStats();
}
