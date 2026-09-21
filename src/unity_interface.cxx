#include "unity_interface.h"
#include "graphics_backend.h"
#include "layer_stack.h"
#include "log.h"

#include <atomic>
#include <cstdint>

namespace {

    /// <summary>
    /// Stores the Unity plugin interface.
    /// </summary>
    IUnityInterfaces* g_unityInterfaces = nullptr;

    /// <summary>
    /// Stores the Unity graphics interface.
    /// </summary>
    IUnityGraphics* g_unityGraphics = nullptr;

    /// <summary>
    /// Stores the base event ID.
    /// </summary>
    /// <seelaso cref="RenderEvent" />
    std::atomic<int> g_eventIdBase{-1};

    /// <summary>
    /// Initializes a new backend instance.
    /// </summary>
    /// <param name="renderer">The renderer for which to create the backend.</param>
    /// <returns>A pointer to the backend instance, or `nullptr`, if the provided renderer is not supported.</returns>
    std::shared_ptr<GraphicsBackend> CreateBackend(UnityGfxRenderer renderer) {
        switch (renderer) {
        case kUnityGfxRendererD3D11:
            return CreateD3D11Backend(*g_unityInterfaces);
        case kUnityGfxRendererD3D12:
            return CreateD3D12Backend(*g_unityInterfaces);
        default:
            LogEvent("graphics API not supported (UnityGfxRenderer %d)", static_cast<int>(renderer));
            return nullptr;
        }
    }

    /// <summary>
    /// Implements a callback for graphics device events used to create and assign or release the requested backend from the layer stack.
    /// </summary>
    /// <param name="eventType">The graphics device event type.</param>
    void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType) {
        switch (eventType) {
        case kUnityGfxDeviceEventInitialize: {
            const UnityGfxRenderer renderer = g_unityGraphics->GetRenderer();

            if (renderer == kUnityGfxRendererNull)
                return;

            auto backend = CreateBackend(renderer);

            LogEvent("graphics device initialized (UnityGfxRenderer %d), backend %s", static_cast<int>(renderer), backend ? "created" : "unavailable");
            g_layers.SetBackend(std::move(backend));

            break;
        }
        case kUnityGfxDeviceEventShutdown:
            LogEvent("graphics device shutdown");
            g_layers.SetBackend(nullptr);

            break;
        default:
            break;
        }
    }

    /// <summary>
    /// Implements a callback for render events, invoked from Unity through `CommandBuffer.IssuePluginEventAndData`.
    /// </summary>
    /// <param name="eventId">The event index.</param>
    /// <param name="data">The event data.</param>
    /// <seealso cref="RenderEvent" />
    void UNITY_INTERFACE_API OnRenderEvent(int eventId, void* data) {
        auto base = g_eventIdBase.load(std::memory_order_relaxed);

        if (base < 0)
            return;

        switch (static_cast<RenderEvent>(eventId - base)) {
        case RenderEvent::EndLayerFrame:
            g_layers.OnEndLayerFrame(static_cast<VarjoLayers_Handle>(reinterpret_cast<intptr_t>(data)));
            break;
        case RenderEvent::CommitLayerViews:
            g_layers.OnCommitLayerViews(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(data)));
            break;
        default:
            break;
        }
    }

}

bool IsUnityPluginLoaded() {
    return g_unityInterfaces != nullptr;
}

UnityRenderingEventAndData VarjoLayers_GetRenderEventFunc() {
    return OnRenderEvent;
}

int VarjoLayers_GetRenderEventBase() {
    return g_eventIdBase.load();
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces) {
    // Resolve and store the plugin and graphics interfaces.
    g_unityInterfaces = unityInterfaces;
    g_unityGraphics = unityInterfaces->Get<IUnityGraphics>();

    // Allocate the space for the render events.
    g_eventIdBase.store(g_unityGraphics->ReserveEventIDRange(static_cast<int>(RenderEvent::Count)));

    // Register the device event callback.
    g_unityGraphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);

    // Issue the graphics device event once to initialize the backend.
    ::OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload() {
    // Release the device event callback.
    if (g_unityGraphics)
        g_unityGraphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);

    // Reset the backend on the layer stack.
    g_layers.SetBackend(nullptr);

    // Reset interface pointers and the event ID range.
    g_eventIdBase.store(-1);
    g_unityGraphics = nullptr;
    g_unityInterfaces = nullptr;
}
