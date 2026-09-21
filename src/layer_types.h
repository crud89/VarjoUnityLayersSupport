#pragma once

#include <cstdint>

/// <summary>
/// Defines the type of a layer handle.
/// </summary>
using VarjoLayers_Handle = int32_t;

/// <summary>
/// Stores the result code returned by various functions of the plugin that may fail.
/// </summary>
enum class VarjoLayers_Result : int32_t {
    /// <summary>
    /// Indicates that no error has occurred.
    /// </summary>
    Ok = 0,

    /// <summary>
    /// Indicates that a handle could not resolve to a layer.
    /// </summary>
    InvalidLayer = -1,

    /// <summary>
    /// Indicates an invalid function argument.
    /// </summary>
    InvalidArgument = -2,

    /// <summary>
    /// Indicates that a layer index was above the supported number of layers.
    /// </summary>
    TooManyLayers = -3,

    /// <summary>
    /// Indicates that a view index could not resolve to a view.
    /// </summary>
    InvalidView = -4,

    /// <summary>
    /// Indicates that a resource is incompatible with the active graphics backend.
    /// </summary>
    WrongGraphicsApi = -5,

    /// <summary>
    /// Indicates that a texture format was unsupported.
    /// </summary>
    UnsupportedFormat = -6,

    /// <summary>
    /// Indicates that a texture layout was unsupported.
    /// </summary>
    UnsupportedLayout = -7,

    /// <summary>
    /// Indicates that a graphics backend is not supported.
    /// </summary>
    NoGraphicsBackend = -8,
};

/// <summary>
/// Defines the texture usage types.
/// </summary>
enum class TextureUsage : int32_t {
    /// <summary>
    /// Indicates that the texture is used as a color texture.
    /// </summary>
    Color = 0,
    
    /// <summary>
    /// Indicates that the texture is used as a depth texture.
    /// </summary>
    Depth = 1,
    
    /// <summary>
    /// Stores the number of supported texture usage types.
    /// </summary>
    Count
};

/// <summary>
/// Indicates the source of a layer's view and projection matrices.
/// </summary>
enum class MatrixSource : int32_t {
    /// <summary>
    /// Indicates that the matrices are obtained from the layer through the Varjo API.
    /// </summary>
    Layer = 0,

    /// <summary>
    /// Indicates that the matrices are supplied by the application through the render layer properties.
    /// </summary>
    Custom = 1,
};

/// <summary>
/// Stores the configuration of the application base layer.
/// </summary>
/// <remarks>
/// The base layer is the singular layer, the Varjo wrapper for Unity (VarjoUnityXR.dll) uses and draws to.
/// </remarks>
struct alignas(8) VarjoApplicationBaseLayerDesc {
    /// <summary>
    /// The flags to set for this layer.
    /// </summary>
    int64_t setFlags;

    /// <summary>
    /// The flags to clear with this layer.
    /// </summary>
    int64_t clearFlags;
};

static_assert(sizeof(VarjoApplicationBaseLayerDesc) == 16);

/// <summary>
/// Describes a custom layer to render into.
/// </summary>
struct alignas(8) VarjoLayerDesc {
    /// <summary>
    /// The order of the layer. Values beneath `0` render below the application layer and values greater or equal to `0` render above.
    /// </summary>
    int64_t order;

    /// <summary>
    /// The enabled-state of the layer. A layer is considered enabled, if this value does not equal `0`.
    /// </summary>
    int32_t enabled;

    /// <summary>
    /// The source of the per-view matices for this layer.
    /// </summary>
    MatrixSource matrixSource;

    /// <summary>
    /// The flags for this layer (varjo_LayerFlags).
    /// </summary>
    int64_t flags;

    /// <summary>
    /// The space of the layer (varjo_Space). Only used if <see cref="matrixSource" /> is <see cref="MatrixSource::Custom" />.
    /// </summary>
    int64_t space;

    /// <summary>
    /// If set to something other than `0`, the layer will submit a depth texture.
    /// </summary>
    int32_t depthEnabled;

    /// <summary>
    /// If set to something other than `0`, the layer will submit a depth range (requires <see cref="depthEnabled" /> to be set as well).
    /// </summary>
    int32_t depthTestRangeEnabled;

    /// <summary>
    /// The minimum depth range value.
    /// </summary>
    double minDepth;

    /// <summary>
    /// The maximum depth range value.
    /// </summary>
    double maxDepth;

    /// <summary>
    /// The distance to the near clipping plane.
    /// </summary>
    double nearZ;

    /// <summary>
    /// The distance to the far clipping plane.
    /// </summary>
    double farZ;

    /// <summary>
    /// The start of the depth test range in meters.
    /// </summary>
    double depthTestNearZ;

    /// <summary>
    /// The end of the depth test range in meters.
    /// </summary>
    double depthTestFarZ;
};

static_assert(sizeof(VarjoLayerDesc) == 88);

/// <summary>
/// The per-view matrices for a layer.
/// </summary>
struct VarjoLayerView {
    /// <summary>
    /// The projection matrix.
    /// </summary>
    double projection[16];

    /// <summary>
    /// The view matrix.
    /// </summary>
    double view[16];
};

static_assert(sizeof(VarjoLayerView) == 256);

/// <summary>
/// Describes the results from a frame submission.
/// </summary>
enum class FrameResult : int32_t {
    /// <summary>
    /// The frame was successfully processed by the layer stack.
    /// </summary>
    Composed = 0,

    /// <summary>
    /// Indicates that the layer stack is disabled.
    /// </summary>
    /// <seealso cref="VarjoLayers_SetEnabled" />
    Disabled = 1,

    /// <summary>
    /// Indicates that the underlying Varjo runtime (VarjoLib.dll) misses the functions for the active graphics backend.
    /// </summary>
    ApiUnavailable = 2,

    /// <summary>
    /// Indicates that the Varjo native plugin (VarjoUntiyXR.dll) does not provide an application base layer.
    /// </summary>
    /// <seealso cref="VarjoLayers_SetApplicationBaseLayer" />
    NoApplicationLayer = 3,

    /// <summary>
    /// Indicates that the  Varjo native plugin (VarjoUnityXR.dll) does provide multiple application base layers.
    /// </summary>
    /// <seealso cref="VarjoLayers_SetApplicationBaseLayer" />
    MultipleApplicationLayers = 4,

    /// <summary>
    /// Indicates that there are more user-defined layers than supported.
    /// </summary>
    TooManyLayers = 5,

    /// <summary>
    /// Indicates that the plug-in does not support the active graphics API.
    /// </summary>
    NoGraphicsBackend = 6,
};

/// <summary>
/// Reports statistics about a frame submission.
/// </summary>
/// <seealso cref="VarjoLayers_GetFrameStats" />
struct alignas(8) VarjoFrameStats {
    /// <summary>
    /// The total number of frames that have been submitted to the layer stack.
    /// </summary>
    /// <remarks>
    /// This value is the sum of <see cref="framesComposited" /> and <see cref="framesPassedThrough" />.
    /// </remarks>
    uint64_t framesTotal;

    /// <summary>
    /// The number of frames that have been composed by the layer stack.
    /// </summary>
    uint64_t framesComposited;

    /// <summary>
    /// The number of frames that have passed-through the application base layer without composing user-defined layers. A frame is passed through if
    /// <see cref="lastResult" /> of a frame is different than <see cref="FrameResult::Composed" />.
    /// </summary>
    uint64_t framesPassedThrough;

    /// <summary>
    /// The flags set by the application base layer from the Varjo plugin (VarjoUnityXR.dll). Set to `-1`, if no base layer is specified.
    /// </summary>
    int64_t applicationLayerFlags;

    /// <summary>
    /// The number of <see cref="RenderEvent::EndLayerFrame" /> events received.
    /// </summary>
    uint64_t endOfFrameEvents;

    /// <summary>
    /// The number of submissions to the frame layer without an <see cref="RenderEvent::EndLayerFrame" /> event. Values different from `0` indicate a bug 
    /// in the user's submission logic.
    /// </summary>
    uint64_t unfencedSubmits;

    /// <summary>
    /// The result of the last full frame submission.
    /// </summary>
    FrameResult lastResult;

    /// <summary>
    /// The number of views in the application base layer.
    /// </summary>
    /// <remarks>
    /// This is typically `2`, if foveated rendering is not used or `4`, if foveated rendering is used.
    /// </remarks>
    int32_t viewCount;

    /// <summary>
    /// Indicates the graphics backend used by the renderer. Maps to `UnityGfxRenderer`. Set to `-1` if the backend is not available.
    /// </summary>
    int32_t renderer;

    /// <summary>
    /// The number of layers in the layer stack.
    /// </summary>
    int32_t layerCount;
};

static_assert(sizeof(VarjoFrameStats) == 64);

/// <summary>
/// Describes the result of a layer submission.
/// </summary>
enum class LayerResult : int32_t {
    /// <summary>
    /// The layer is still pending in the stack.
    /// </summary>
    Pending = -1,

    /// <summary>
    /// The layer is submitted to the stack.
    /// </summary>
    Submitted = 0,

    /// <summary>
    /// The layer is disabled.
    /// </summary>
    Disabled = 1,

    /// <summary>
    /// The layer is set to <see cref="MatrixSource::Custom" /> but the <see cref="RenderEvent::CommitLayerViews" /> did not provide view matrices.
    /// </summary>
    NoViews = 2,

    /// <summary>
    /// The layer uses more views than the runtime supports.
    /// </summary>
    TooManyViews = 3,

    /// <summary>
    /// Indicates a missing texture (color, or depth if depth is enabled).
    /// </summary>
    MissingTexture = 4,

    /// <summary>
    /// Creating the swap chain from the Varjo library failed for the layer textures.
    /// </summary>
    SwapChainFailed = 5,

    /// <summary>
    /// Copying the texture into the Varjo back buffer failed.
    /// </summary>
    CopyFailed = 6,

    /// <summary>
    /// Indicates that no <see cref="RenderEvent::EndLayerFrame" /> event was received for the frame.
    /// </summary>
    NotPrepared = 7,
};

/// <summary>
/// Reports statistics about a layer submission.
/// </summary>
/// <seealso cref="VarjoLayers_GetLayerStats" />
struct alignas(8) VarjoLayerStats {
    /// <summary>
    /// The number of frames this layer was submitted to.
    /// </summary>
    uint64_t framesSubmitted;

    /// <summary>
    /// The number of frames this layer was not submitted to.
    /// </summary>
    uint64_t framesSkipped;

    /// <summary>
    /// The result of the last layer submission.
    /// </summary>
    LayerResult lastResult;

    /// <summary>
    /// The number of views in the layer.
    /// </summary>
    int32_t viewCount;
};

static_assert(sizeof(VarjoLayerStats) == 24);
