#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <d3d12.h>

#include <Varjo.h>
#include <Varjo_layers.h>
#include <Varjo_d3d11.h>
#include <Varjo_d3d12.h>

#include <IUnityInterface.h>
#include <IUnityGraphics.h>
#include <IUnityGraphicsD3D11.h>
#include <IUnityGraphicsD3D12.h>

#include "layer_types.h"

#define VARJO_XR_LAYERS_SUPPORT_API extern "C" __declspec(dllexport)

/// <summary>
/// Toggles all layers in the plugin.
/// </summary>
/// <param name="enabled">`true` if the layers should be enabled and `false` otherwise.</param>
VARJO_XR_LAYERS_SUPPORT_API void VarjoLayers_SetEnabled(bool enabled);

/// <summary>
/// Sets the application's base layer.
/// </summary>
/// <remarks>
/// The base layer is the layer the Varjo wrapper for Unity (VarjoUnityXR.dll) renders into through the XR rig interface.
/// </remarks>
/// <param name="desc">A pointer to the base layer description.</param>
/// <returns>The result of the operation.</returns>
VARJO_XR_LAYERS_SUPPORT_API VarjoLayers_Result VarjoLayers_SetApplicationBaseLayer(const VarjoApplicationBaseLayerDesc* desc);

/// <summary>
/// Creates a new layer.
/// </summary>
/// <param name="desc">The layer description.</param>
/// <returns>The handle of the layer.</returns>
VARJO_XR_LAYERS_SUPPORT_API VarjoLayers_Handle VarjoLayers_CreateLayer(const VarjoLayerDesc* desc);

/// <summary>
/// Updates a layer.
/// </summary>
/// <param name="layer">The handle of the layer to update.</param>
/// <param name="desc">The layer description.</param>
/// <returns>The reslt of the operation.</returns>
VARJO_XR_LAYERS_SUPPORT_API VarjoLayers_Result VarjoLayers_UpdateLayer(VarjoLayers_Handle layer, const VarjoLayerDesc* desc);

/// <summary>
/// Destroys a layer.
/// </summary>
/// <param name="layer">The handle of the layer to destroy.</param>
/// <returns>The result of the operation.</returns>
VARJO_XR_LAYERS_SUPPORT_API VarjoLayers_Result VarjoLayers_DestroyLayer(VarjoLayers_Handle layer);

/// <summary>
/// Sets the target texture for a layer to render into.
/// </summary>
/// <param name="layer">The handle of the layer to update.</param>
/// <param name="view">The index of the view within the layer.</param>
/// <param name="usage">The intended usage of the target texture.</param>
/// <param name="nativeTexture">A pointer to the native texture, the layer renders into.</param>
/// <returns>The result of the operation.</returns>
VARJO_XR_LAYERS_SUPPORT_API VarjoLayers_Result VarjoLayers_SetLayerTexture(VarjoLayers_Handle layer, int32_t view, TextureUsage usage, void* nativeTexture);

/// <summary>
/// Clears the texture in a layer.
/// </summary>
/// <param name="layer">The handle of the layer on which to clear the textures.</param>
/// <returns>The result of the operation.</returns>
VARJO_XR_LAYERS_SUPPORT_API VarjoLayers_Result VarjoLayers_ClearLayerTextures(VarjoLayers_Handle layer);

/// <summary>
/// Pushes a new set of view matrices for a layer that uses <see cref="MatrixSource::Custom" />.
/// </summary>
/// <param name="layer">The handle of the layer to provide the matrices for.</param>
/// <param name="views">A pointer to the array of views.</param>
/// <param name="viewCount">The number of views in <paramref name="views" />.</param>
/// <returns>A token that must be passed along the <see cref="RenderEvent::CommitLayerViews" /> event when calling `CommandBuffer.IssuePluginEventAndData` from Unity. A value of `0` indicates that this operation has failed.</returns>
VARJO_XR_LAYERS_SUPPORT_API uint32_t VarjoLayers_StageLayerViews(VarjoLayers_Handle layer, const VarjoLayerView* views, int32_t viewCount);

/// <summary>
/// Returns the view matrices of the application's base layer.
/// </summary>
/// <param name="views">A pointer to an array that receives the view matrices. Can be set to `nullptr` to only obtain the number of views in the base layer.</param>
/// <param name="capacity">The size of the array provided with <paramref name="views" />.</param>
/// <returns>The number of views in the application base layer.</returns>
VARJO_XR_LAYERS_SUPPORT_API int32_t VarjoLayers_GetApplicationBaseLayerViews(VarjoLayerView* views, int32_t capacity);

/// <summary>
/// Acquires the statistics about the last frame submission.
/// </summary>
/// <param name="stats">A pointer to a structure that receives the frame statistics.</param>
VARJO_XR_LAYERS_SUPPORT_API void VarjoLayers_GetFrameStats(VarjoFrameStats* stats);

/// <summary>
/// Acquires the statistics about the last layer submission.
/// </summary>
/// <param name="layer">The layer for which to acquire the statistics.</param>
/// <param name="stats">The statistics of the layer.</param>
/// <returns>The result of the operation.</returns>
VARJO_XR_LAYERS_SUPPORT_API VarjoLayers_Result VarjoLayers_GetLayerStats(VarjoLayers_Handle layer, VarjoLayerStats* stats);

/// <summary>
/// Resets the counters in the frame and layer statistics.
/// </summary>
VARJO_XR_LAYERS_SUPPORT_API void VarjoLayers_ResetStats();
