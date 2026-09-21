#pragma once

#include "plugin.h"
#include "graphics_backend.h"
#include "layer_types.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

class LayerStack {
public:
    static constexpr int32_t kMaxLayers = 8;
    static constexpr int32_t kMaxViews = 4;
    static constexpr int32_t kMaxSubmittedLayers = 16;
    static constexpr int32_t kStagingSlots = 16;

    using EndFrameWithLayersFn = decltype(&varjo_EndFrameWithLayers);

    LayerStack();

    void SetEnabled(bool enabled) { 
        m_enabled.store(enabled); 
    }
    
    bool HasBackend() const { 
        return m_renderer.load() >= 0; 
    }

    void SetBackend(std::shared_ptr<GraphicsBackend> backend);
    void Reset();

    VarjoLayers_Result SetApplicationLayer(const VarjoApplicationBaseLayerDesc* desc);
    VarjoLayers_Handle CreateLayer(const VarjoLayerDesc* desc);
    VarjoLayers_Result UpdateLayer(VarjoLayers_Handle layer, const VarjoLayerDesc* desc);
    VarjoLayers_Result DestroyLayer(VarjoLayers_Handle layer);
    VarjoLayers_Result SetLayerTexture(VarjoLayers_Handle layer, int32_t view, TextureUsage usage, void* nativeTexture);
    VarjoLayers_Result ClearLayerTextures(VarjoLayers_Handle layer);
    uint32_t PushLayerViewMatrices(VarjoLayers_Handle layer, const VarjoLayerView* views, int32_t viewCount);
    int32_t GetApplicationBaseLayerViews(VarjoLayerView* views, int32_t capacity) const;
    VarjoFrameStats GetFrameStats() const;
    VarjoLayers_Result GetLayerStats(VarjoLayers_Handle layer, VarjoLayerStats* stats) const;
    void ResetStats();

    void OnEndLayerFrame(VarjoLayers_Handle layer);
    void OnCommitLayerViews(uint32_t token);
    void Submit(varjo_Session* session, varjo_SubmitInfoLayers* info, EndFrameWithLayersFn endFrame);

    void OnSessionShutDown();

private:
    static constexpr int32_t kUsageCount = static_cast<int32_t>(TextureUsage::Count);
    static constexpr int32_t kMaxRenderLayers = 2 * kMaxLayers;

    using ViewTextures = std::array<SourceTexture, kUsageCount>;

    struct Layer {
        VarjoLayers_Handle id = 0;
        int32_t slot = 0;
        uint64_t serial = 0;
        VarjoLayerDesc desc{};
        std::array<ViewTextures, kMaxViews> textures;
        uint64_t textureGeneration = 0;
    };

    struct Snapshot {
        VarjoApplicationBaseLayerDesc application{};
        std::vector<Layer> layers;
    };

    struct RenderLayer {
        int32_t id = 0;
        std::array<std::array<SwapChain, kUsageCount>, kMaxViews> swapChains{};
        uint64_t failedGeneration = UINT64_MAX;
        uint64_t preparedSubmit = UINT64_MAX;
        uint64_t preparedGeneration = UINT64_MAX;
        int32_t committedViewCount = 0;
        std::array<VarjoLayerView, kMaxViews> committedViews{};
    };

    struct StagedViews {
        uint32_t token = 0;
        int32_t layer = 0;
        int32_t viewCount = 0;
        std::array<VarjoLayerView, kMaxViews> views{};
    };

    struct PreparedLayer {
        const Layer* layer = nullptr;
        RenderLayer* render = nullptr;
        int32_t viewCount = 0;
    };

    struct LayerCounters {
        std::atomic<uint64_t> submitted{0};
        std::atomic<uint64_t> skipped{0};
        std::atomic<int32_t> lastResult{static_cast<int32_t>(LayerResult::Pending)};
        std::atomic<int32_t> viewCount{0};
    };

    struct FrameScratch {
        std::array<PreparedLayer, kMaxLayers> prepared{};
        std::array<varjo_LayerMultiProj, kMaxLayers> layers{};
        std::array<std::array<varjo_LayerMultiProjView, kMaxViews>, kMaxLayers> views{};
        std::array<std::array<varjo_ViewExtensionDepth, kMaxViews>, kMaxLayers> depth{};
        std::array<std::array<varjo_ViewExtensionDepthTestRange, kMaxViews>, kMaxLayers> depthTestRange{};
        std::array<CopyRequest, kMaxLayers * kMaxViews * kUsageCount> copies{};
        std::array<varjo_SwapChain*, kMaxLayers * kMaxViews * kUsageCount> acquired{};
        std::array<varjo_LayerHeader*, kMaxSubmittedLayers> headers{};
        varjo_LayerMultiProj application{};
    };

    Layer* FindLayer(VarjoLayers_Handle id);
    const Layer* FindLayer(VarjoLayers_Handle id) const;
    void Publish();

    std::shared_ptr<const Snapshot> LoadSnapshot() const;

    FrameResult SubmitStack(varjo_Session* session, varjo_SubmitInfoLayers* info, EndFrameWithLayersFn endFrame);
    LayerResult PrepareLayer(varjo_Session* session, const Layer& layer, const varjo_LayerMultiProj& application, PreparedLayer& prepared);
    bool EnsureSwapChains(varjo_Session* session, const Layer& layer, RenderLayer& render, int32_t viewCount);
    void BuildLayer(int32_t index, const PreparedLayer& prepared, const varjo_LayerMultiProj& application);
    RenderLayer* FindOrAddRenderLayer(int32_t id);
    void RemoveStaleRenderLayers(const Snapshot& snapshot);
    void FreeSwapChain(SwapChain& swapChain);
    void FreeSwapChains(RenderLayer& render);
    void FreeAllSwapChains();

    void RecordApplicationLayer(const varjo_LayerMultiProj& application);
    void RecordFrameOutcome(FrameResult outcome);
    void RecordLayerOutcome(const Layer& layer, LayerResult outcome, int32_t viewCount);

    mutable std::mutex m_mainMutex;
    std::array<Layer, kMaxLayers> m_layers;
    VarjoApplicationBaseLayerDesc m_application{};
    int32_t m_nextId = 1;
    uint64_t m_nextSerial = 1;

    mutable std::mutex m_snapshotMutex;
    std::shared_ptr<const Snapshot> m_snapshot;

    mutable std::mutex m_backendMutex;
    std::shared_ptr<GraphicsBackend> m_mainThreadBackend;

    mutable std::mutex m_stagingMutex;
    std::array<StagedViews, kStagingSlots> m_staging{};
    uint32_t m_nextToken = 1;

    std::mutex m_renderMutex;
    std::shared_ptr<GraphicsBackend> m_backend;
    std::array<RenderLayer, kMaxRenderLayers> m_renderLayers{};
    varjo_Session* m_session = nullptr;
    uint64_t m_markersAtLastSubmit = 0;
    std::atomic<uint64_t> m_submitIndex{0};
    FrameScratch m_scratch{};
    
    mutable std::mutex m_applicationViewsMutex;
    std::array<VarjoLayerView, kMaxViews> m_applicationViews{};
    int32_t m_applicationViewCount = 0;

    std::atomic<bool> m_enabled{false};
    std::array<LayerCounters, kMaxLayers> m_counters;

    std::atomic<uint64_t> m_framesTotal{0};
    std::atomic<uint64_t> m_framesComposited{0};
    std::atomic<uint64_t> m_framesPassedThrough{0};
    std::atomic<int64_t> m_applicationFlags{-1};
    std::atomic<uint64_t> m_frameMarkers{0};
    std::atomic<uint64_t> m_submitsWithoutMarker{0};
    std::atomic<int32_t> m_lastOutcome{static_cast<int32_t>(FrameResult::Disabled)};
    std::atomic<int32_t> m_renderer{-1};
    std::atomic<int32_t> m_layerCount{0};
    std::atomic<uint64_t> m_stagingMisses{0};
};

extern LayerStack g_layers;
