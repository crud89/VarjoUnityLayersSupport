#pragma once

#include "plugin.h"

/// <summary>
/// Defines the render events used by the plugin.
/// </summary>
/// <seealso cref="VarjoLayers_GetRenderEventBase" />
enum class RenderEvent : int {
    /// <summary>
    /// Indicates that the rendering of a frame for a layer has been finished. The event data is the layer handle.
    /// </summary>
    EndLayerFrame = 0,

    /// <summary>
    /// Applies staged layer views. The event data is the token returned by <see cref="VarjoLayers_StageLayerViews" />.
    /// </summary>
    CommitLayerViews = 1,

    /// <summary>
    /// The number of render events supported by the plugin.
    /// </summary>
    Count
};

/// <summary>
/// Checks if the plugin is loaded by the Unity runtime.
/// </summary>
/// <returns>`true` if the plugin is loaded by the Unity runtime and `false` otherwise.</returns>
VARJO_XR_LAYERS_SUPPORT_API bool IsUnityPluginLoaded();

/// <summary>
/// Returns the render event callback of the plugin.
/// </summary>
/// <returns>A function pointer to the render event callback.</returns>
VARJO_XR_LAYERS_SUPPORT_API UnityRenderingEventAndData VarjoLayers_GetRenderEventFunc();

/// <summary>
/// Returns the base event ID for the <see cref="RenderEvent" /> mapping.
/// </summary>
/// <returns>The base ID at which the first render event is mapped.</returns>
/// <seealso cref="RenderEvent" />
VARJO_XR_LAYERS_SUPPORT_API int VarjoLayers_GetRenderEventBase();
