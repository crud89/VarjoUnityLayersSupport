#include "layer_stack.h"
#include "log.h"
#include "varjo_api.h"

#include <algorithm>
#include <climits>
#include <span>

LayerStack g_layers;

namespace {

    constexpr int32_t kColor = static_cast<int32_t>(TextureUsage::Color);
    constexpr int32_t kDepth = static_cast<int32_t>(TextureUsage::Depth);

    const char* ToString(FrameResult outcome) {
        switch (outcome) {
        case FrameResult::Composed: return "composited";
        case FrameResult::Disabled: return "disabled";
        case FrameResult::ApiUnavailable: return "Varjo API unavailable for the active graphics API";
        case FrameResult::NoApplicationLayer: return "no application layer submitted";
        case FrameResult::MultipleApplicationLayers: return "more than one application layer submitted";
        case FrameResult::TooManyLayers: return "too many layers for one submission";
        case FrameResult::NoGraphicsBackend: return "no graphics backend for the active graphics API";
        default: return "unknown";
        }
    }

    const char* ToString(LayerResult outcome) {
        switch (outcome) {
        case LayerResult::Pending: return "pending";
        case LayerResult::Submitted: return "submitted";
        case LayerResult::Disabled: return "disabled";
        case LayerResult::NoViews: return "no views committed";
        case LayerResult::TooManyViews: return "unsupported view count";
        case LayerResult::MissingTexture: return "texture missing for a view";
        case LayerResult::SwapChainFailed: return "swap chain unavailable";
        case LayerResult::CopyFailed: return "copy failed";
        case LayerResult::NotPrepared: return "textures not prepared in this frame (issue EndLayerFrame after rendering)";
        default: return "unknown";
        }
    }

    bool HasBlendControlMask(const varjo_LayerMultiProj& layer) {
        if (!layer.views)
            return false;

        for (int32_t view = 0; view < layer.viewCount; ++view)
            for (const varjo_ViewExtension* extension = layer.views[view].extension; extension; extension = extension->next)
                if (extension->type == varjo_ViewExtensionBlendControlMaskType)
                    return true;

        return false;
    }

    bool IsValid(const VarjoLayerDesc& desc) {
        if (desc.matrixSource != MatrixSource::Layer && desc.matrixSource != MatrixSource::Custom)
            return false;

        if (desc.depthEnabled && (desc.minDepth > desc.maxDepth || desc.nearZ == desc.farZ))
            return false;

        return true;
    }

    varjo_Matrix ToMatrix(const double (&values)[16]) {
        varjo_Matrix matrix{};
        std::copy(std::begin(values), std::end(values), std::begin(matrix.value));

        return matrix;
    }

    void ToView(const varjo_LayerMultiProjView& source, VarjoLayerView& view) {
        std::copy(std::begin(source.projection.value), std::end(source.projection.value), std::begin(view.projection));
        std::copy(std::begin(source.view.value), std::end(source.view.value), std::begin(view.view));
    }

    varjo_SwapChainViewport ToViewport(const SwapChain& swapChain) {
        return {
            .swapChain = swapChain.handle,
            .x = 0,
            .y = 0,
            .width = swapChain.desc.width,
            .height = swapChain.desc.height,
            .arrayIndex = 0,
            .reserved = 0,
        };
    }

}

LayerStack::LayerStack() :
    m_snapshot(std::make_shared<Snapshot>())
{
}

void LayerStack::SetBackend(std::shared_ptr<GraphicsBackend> backend) {
    std::lock_guard renderLock(m_renderMutex);

    FreeAllSwapChains();
    m_backend = backend;
    m_renderer.store(backend ? static_cast<int32_t>(backend->Renderer()) : -1);

    {
        std::lock_guard backendLock(m_backendMutex);
        m_mainThreadBackend = std::move(backend);
    }

    std::lock_guard mainLock(m_mainMutex);

    for (Layer& layer : m_layers) {
        if (layer.id == 0)
            continue;

        layer.textures = {};
        ++layer.textureGeneration;
    }

    Publish();
}

void LayerStack::Reset() {
    m_enabled.store(false);

    {
        std::lock_guard lock(m_mainMutex);
        m_layers = {};
        m_application = VarjoApplicationBaseLayerDesc{ .setFlags = 0, .clearFlags = 0 };
        Publish();
    }

    {
        std::lock_guard lock(m_stagingMutex);
        m_staging = {};
    }

    ResetStats();
}

VarjoLayers_Result LayerStack::SetApplicationLayer(const VarjoApplicationBaseLayerDesc* desc) {
    if (!desc)
        return VarjoLayers_Result::InvalidArgument;

    std::lock_guard lock(m_mainMutex);
    m_application = *desc;

    Publish();

    return VarjoLayers_Result::Ok;
}

VarjoLayers_Handle LayerStack::CreateLayer(const VarjoLayerDesc* desc) {
    if (!desc || !IsValid(*desc))
        return static_cast<int32_t>(VarjoLayers_Result::InvalidArgument);

    std::lock_guard lock(m_mainMutex);
    auto free = std::ranges::find_if(m_layers, [](const Layer& layer) { return layer.id == 0; });

    if (free == m_layers.end())
        return static_cast<int32_t>(VarjoLayers_Result::TooManyLayers);

    const auto slot = static_cast<int32_t>(free - m_layers.begin());
    const int32_t id = m_nextId;
    m_nextId = m_nextId == INT32_MAX ? 1 : m_nextId + 1;

    *free = Layer{};
    free->id = id;
    free->slot = slot;
    free->serial = m_nextSerial++;
    free->desc = *desc;

    LayerCounters& counters = m_counters[slot];
    counters.submitted.store(0);
    counters.skipped.store(0);
    counters.lastResult.store(static_cast<int32_t>(LayerResult::Pending));
    counters.viewCount.store(0);

    Publish();

    return free->id;
}

VarjoLayers_Result LayerStack::UpdateLayer(VarjoLayers_Handle layer, const VarjoLayerDesc* desc) {
    if (!desc || !IsValid(*desc))
        return VarjoLayers_Result::InvalidArgument;

    std::lock_guard lock(m_mainMutex);
    Layer* entry = FindLayer(layer);

    if (!entry)
        return VarjoLayers_Result::InvalidLayer;

    entry->desc = *desc;
    Publish();

    return VarjoLayers_Result::Ok;
}

VarjoLayers_Result LayerStack::DestroyLayer(VarjoLayers_Handle layer) {
    std::lock_guard lock(m_mainMutex);
    Layer* entry = FindLayer(layer);

    if (!entry)
        return VarjoLayers_Result::InvalidLayer;

    *entry = Layer{};
    Publish();

    return VarjoLayers_Result::Ok;
}

VarjoLayers_Result LayerStack::SetLayerTexture(VarjoLayers_Handle layer, int32_t view, TextureUsage usage, void* nativeTexture) {
    if (view < 0 || view >= kMaxViews)
        return VarjoLayers_Result::InvalidView;

    if (usage != TextureUsage::Color && usage != TextureUsage::Depth)
        return VarjoLayers_Result::InvalidArgument;

    SourceTexture texture;

    if (nativeTexture) {
        std::shared_ptr<GraphicsBackend> backend;

        {
            std::lock_guard lock(m_backendMutex);
            backend = m_mainThreadBackend;
        }

        if (!backend)
            return VarjoLayers_Result::NoGraphicsBackend;

        if (const VarjoLayers_Result result = backend->Register(nativeTexture, usage, texture); result != VarjoLayers_Result::Ok)
            return result;
    }

    std::lock_guard lock(m_mainMutex);
    Layer* entry = FindLayer(layer);

    if (!entry)
        return VarjoLayers_Result::InvalidLayer;

    entry->textures[view][static_cast<int32_t>(usage)] = std::move(texture);
    ++entry->textureGeneration;
    Publish();

    return VarjoLayers_Result::Ok;
}

VarjoLayers_Result LayerStack::ClearLayerTextures(VarjoLayers_Handle layer) {
    std::lock_guard lock(m_mainMutex);
    Layer* entry = FindLayer(layer);

    if (!entry)
        return VarjoLayers_Result::InvalidLayer;

    entry->textures = {};
    ++entry->textureGeneration;
    Publish();

    return VarjoLayers_Result::Ok;
}

uint32_t LayerStack::PushLayerViewMatrices(VarjoLayers_Handle layer, const VarjoLayerView* views, int32_t viewCount) {
    if (!views || viewCount <= 0 || viewCount > kMaxViews)
        return 0;

    {
        std::lock_guard lock(m_mainMutex);

        if (!FindLayer(layer))
            return 0;
    }

    std::lock_guard lock(m_stagingMutex);
    const uint32_t token = m_nextToken++;

    if (m_nextToken == 0)
        m_nextToken = 1;

    StagedViews& staged = m_staging[token % kStagingSlots];
    staged = StagedViews{ .token = token, .layer = layer, .viewCount = viewCount };
    std::copy(views, views + viewCount, staged.views.begin());

    return token;
}

int32_t LayerStack::GetApplicationBaseLayerViews(VarjoLayerView* views, int32_t capacity) const {
    std::lock_guard lock(m_applicationViewsMutex);

    if (views && capacity > 0)
        std::copy_n(m_applicationViews.begin(), std::min(capacity, m_applicationViewCount), views);

    return m_applicationViewCount;
}

VarjoFrameStats LayerStack::GetFrameStats() const {
    int32_t viewCount = 0;

    {
        std::lock_guard lock(m_applicationViewsMutex);
        viewCount = m_applicationViewCount;
    }

    return {
        .framesTotal = m_framesTotal.load(),
        .framesComposited = m_framesComposited.load(),
        .framesPassedThrough = m_framesPassedThrough.load(),
        .applicationLayerFlags = m_applicationFlags.load(),
        .endOfFrameEvents = m_frameMarkers.load(),
        .unfencedSubmits = m_submitsWithoutMarker.load(),
        .lastResult = static_cast<FrameResult>(m_lastOutcome.load()),
        .viewCount = viewCount,
        .renderer = m_renderer.load(),
        .layerCount = m_layerCount.load(),
    };
}

VarjoLayers_Result LayerStack::GetLayerStats(VarjoLayers_Handle layer, VarjoLayerStats* stats) const {
    if (!stats)
        return VarjoLayers_Result::InvalidArgument;

    std::lock_guard lock(m_mainMutex);
    const Layer* entry = FindLayer(layer);

    if (!entry)
        return VarjoLayers_Result::InvalidLayer;

    const LayerCounters& counters = m_counters[entry->slot];

    *stats = VarjoLayerStats{
        .framesSubmitted = counters.submitted.load(),
        .framesSkipped = counters.skipped.load(),
        .lastResult = static_cast<LayerResult>(counters.lastResult.load()),
        .viewCount = counters.viewCount.load(),
    };

    return VarjoLayers_Result::Ok;
}

void LayerStack::ResetStats() {
    m_framesTotal.store(0);
    m_framesComposited.store(0);
    m_framesPassedThrough.store(0);
    m_applicationFlags.store(-1);
    m_frameMarkers.store(0);
    m_submitsWithoutMarker.store(0);
    m_stagingMisses.store(0);

    for (LayerCounters& counters : m_counters) {
        counters.submitted.store(0);
        counters.skipped.store(0);
    }
}

LayerStack::Layer* LayerStack::FindLayer(VarjoLayers_Handle id) {
    if (id <= 0)
        return nullptr;

    auto it = std::ranges::find_if(m_layers, [id](const Layer& layer) { return layer.id == id; });

    return it != m_layers.end() ? &*it : nullptr;
}

const LayerStack::Layer* LayerStack::FindLayer(VarjoLayers_Handle id) const {
    return const_cast<LayerStack*>(this)->FindLayer(id);
}

void LayerStack::Publish() {
    auto snapshot = std::make_shared<Snapshot>();
    snapshot->application = m_application;

    for (const Layer& layer : m_layers)
        if (layer.id != 0)
            snapshot->layers.push_back(layer);

    std::ranges::sort(snapshot->layers, [](const Layer& a, const Layer& b) {
        return a.desc.order != b.desc.order ? a.desc.order < b.desc.order : a.serial < b.serial;
    });

    m_layerCount.store(static_cast<int32_t>(snapshot->layers.size()));

    std::lock_guard lock(m_snapshotMutex);
    m_snapshot = std::move(snapshot);
}

std::shared_ptr<const LayerStack::Snapshot> LayerStack::LoadSnapshot() const {
    std::lock_guard lock(m_snapshotMutex);
    return m_snapshot;
}

void LayerStack::OnEndLayerFrame(VarjoLayers_Handle layer) {
    m_frameMarkers.fetch_add(1, std::memory_order_relaxed);

    if (layer <= 0)
        return;

    const std::shared_ptr<const Snapshot> snapshot = LoadSnapshot();
    auto entry = std::ranges::find_if(snapshot->layers, [layer](const Layer& candidate) { return candidate.id == layer; });

    if (entry == snapshot->layers.end())
        return;

    std::lock_guard lock(m_renderMutex);

    if (!m_backend || !m_backend->RequiresPreparation())
        return;

    RenderLayer* render = FindOrAddRenderLayer(layer);

    if (!render)
        return;

    std::array<CopySource, kMaxViews * kUsageCount> sources{};
    int32_t sourceCount = 0;
    const int32_t usages = entry->desc.depthEnabled ? kUsageCount : 1;

    for (int32_t view = 0; view < kMaxViews; ++view)
        for (int32_t usage = 0; usage < usages; ++usage)
            if (IUnknown* resource = entry->textures[view][usage].resource.Get())
                sources[sourceCount++] = CopySource{ .resource = resource, .usage = static_cast<TextureUsage>(usage) };

    m_backend->PrepareCopySources(std::span(sources.data(), static_cast<size_t>(sourceCount)));

    render->preparedSubmit = m_submitIndex.load(std::memory_order_relaxed);
    render->preparedGeneration = entry->textureGeneration;
}

void LayerStack::OnCommitLayerViews(uint32_t token) {
    StagedViews staged;

    {
        std::lock_guard lock(m_stagingMutex);
        const StagedViews& slot = m_staging[token % kStagingSlots];

        if (slot.token != token) {
            if (m_stagingMisses.fetch_add(1, std::memory_order_relaxed) == 0)
                LogEvent("committed view set %u was already overwritten; stage fewer view sets per frame", token);

            return;
        }

        staged = slot;
    }

    std::lock_guard lock(m_renderMutex);

    if (RenderLayer* render = FindOrAddRenderLayer(staged.layer)) {
        render->committedViewCount = staged.viewCount;
        render->committedViews = staged.views;
    }
}

void LayerStack::Submit(varjo_Session* session, varjo_SubmitInfoLayers* info, EndFrameWithLayersFn endFrame) {
    m_framesTotal.fetch_add(1, std::memory_order_relaxed);

    if (const uint64_t markers = m_frameMarkers.load(std::memory_order_relaxed); markers > 0) {
        if (markers == m_markersAtLastSubmit)
            m_submitsWithoutMarker.fetch_add(1, std::memory_order_relaxed);

        m_markersAtLastSubmit = markers;
    }

    const FrameResult outcome = SubmitStack(session, info, endFrame);
    RecordFrameOutcome(outcome);

    m_submitIndex.fetch_add(1, std::memory_order_relaxed);

    if (outcome == FrameResult::Composed) {
        m_framesComposited.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    m_framesPassedThrough.fetch_add(1, std::memory_order_relaxed);
    endFrame(session, info);
}

FrameResult LayerStack::SubmitStack(varjo_Session* session, varjo_SubmitInfoLayers* info, EndFrameWithLayersFn endFrame) {
    if (!info || !info->layers || info->layerCount <= 0)
        return FrameResult::NoApplicationLayer;

    int32_t applicationIndex = -1;

    for (int32_t i = 0; i < info->layerCount; ++i) {
        varjo_LayerHeader* header = info->layers[i];

        if (!header || header->type != varjo_LayerMultiProjType)
            continue;

        if (HasBlendControlMask(*reinterpret_cast<varjo_LayerMultiProj*>(header)))
            continue;

        if (applicationIndex >= 0)
            return FrameResult::MultipleApplicationLayers;

        applicationIndex = i;
    }

    if (applicationIndex < 0)
        return FrameResult::NoApplicationLayer;

    const auto& application = *reinterpret_cast<varjo_LayerMultiProj*>(info->layers[applicationIndex]);

    RecordApplicationLayer(application);

    if (!m_enabled.load(std::memory_order_relaxed))
        return FrameResult::Disabled;

    const std::shared_ptr<const Snapshot> snapshot = LoadSnapshot();

    std::lock_guard lock(m_renderMutex);

    if (!m_backend)
        return FrameResult::NoGraphicsBackend;

    if (!g_varjo.IsResolved() || !m_backend->HasVarjoSupport())
        return FrameResult::ApiUnavailable;

    if (session != m_session) {
        FreeAllSwapChains();
        m_session = session;
    }

    RemoveStaleRenderLayers(*snapshot);

    if (info->layerCount + static_cast<int32_t>(snapshot->layers.size()) > kMaxSubmittedLayers)
        return FrameResult::TooManyLayers;

    int32_t preparedCount = 0;

    for (const Layer& layer : snapshot->layers) {
        PreparedLayer& prepared = m_scratch.prepared[preparedCount];

        if (const LayerResult outcome = PrepareLayer(session, layer, application, prepared); outcome == LayerResult::Submitted)
            ++preparedCount;
        else
            RecordLayerOutcome(layer, outcome, 0);
    }

    int32_t copyCount = 0;

    for (int32_t index = 0; index < preparedCount; ++index) {
        const PreparedLayer& prepared = m_scratch.prepared[index];
        const int32_t usages = prepared.layer->desc.depthEnabled ? kUsageCount : 1;

        for (int32_t view = 0; view < prepared.viewCount; ++view) {
            for (int32_t usage = 0; usage < usages; ++usage) {
                const SwapChain& swapChain = prepared.render->swapChains[view][usage];
                int32_t imageIndex = 0;
                g_varjo.AcquireSwapChainImage(swapChain.handle, &imageIndex);

                if (imageIndex < 0 || imageIndex >= kSwapChainLength)
                    imageIndex = 0;

                m_scratch.acquired[copyCount] = swapChain.handle;
                m_scratch.copies[copyCount] = {
                    .source = prepared.layer->textures[view][usage].resource.Get(),
                    .destination = swapChain.images[imageIndex],
                    .usage = static_cast<TextureUsage>(usage),
                };

                ++copyCount;
            }
        }
    }

    const bool copied = copyCount == 0 || m_backend->Copy(std::span(m_scratch.copies.data(), static_cast<size_t>(copyCount)));

    for (int32_t index = 0; index < copyCount; ++index)
        g_varjo.ReleaseSwapChainImage(m_scratch.acquired[index]);

    if (!copied) {
        for (int32_t index = 0; index < preparedCount; ++index)
            RecordLayerOutcome(*m_scratch.prepared[index].layer, LayerResult::CopyFailed, 0);

        preparedCount = 0;
    }

    for (int32_t index = 0; index < preparedCount; ++index)
        BuildLayer(index, m_scratch.prepared[index], application);

    m_scratch.application = application;
    m_scratch.application.header.flags = (application.header.flags | snapshot->application.setFlags) & ~snapshot->application.clearFlags;

    int32_t headerCount = 0;

    for (int32_t i = 0; i < info->layerCount; ++i) {
        if (i != applicationIndex) {
            m_scratch.headers[headerCount++] = info->layers[i];
            continue;
        }

        for (int32_t index = 0; index < preparedCount; ++index)
            if (m_scratch.prepared[index].layer->desc.order < 0)
                m_scratch.headers[headerCount++] = &m_scratch.layers[index].header;

        m_scratch.headers[headerCount++] = &m_scratch.application.header;

        for (int32_t index = 0; index < preparedCount; ++index)
            if (m_scratch.prepared[index].layer->desc.order >= 0)
                m_scratch.headers[headerCount++] = &m_scratch.layers[index].header;
    }

    varjo_SubmitInfoLayers submitInfo = *info;
    submitInfo.layerCount = headerCount;
    submitInfo.layers = m_scratch.headers.data();
    endFrame(session, &submitInfo);

    for (int32_t index = 0; index < preparedCount; ++index)
        RecordLayerOutcome(*m_scratch.prepared[index].layer, LayerResult::Submitted, m_scratch.prepared[index].viewCount);

    return FrameResult::Composed;
}

LayerResult LayerStack::PrepareLayer(varjo_Session* session, const Layer& layer, const varjo_LayerMultiProj& application, PreparedLayer& prepared) {
    if (!layer.desc.enabled)
        return LayerResult::Disabled;

    RenderLayer* render = FindOrAddRenderLayer(layer.id);

    if (!render)
        return LayerResult::SwapChainFailed;

    const bool custom = layer.desc.matrixSource == MatrixSource::Custom;
    const int32_t viewCount = custom ? render->committedViewCount : application.viewCount;

    if (custom && viewCount == 0)
        return LayerResult::NoViews;

    if (viewCount <= 0 || viewCount > kMaxViews)
        return LayerResult::TooManyViews;

    for (int32_t view = 0; view < viewCount; ++view) {
        if (!layer.textures[view][kColor].resource)
            return LayerResult::MissingTexture;

        if (layer.desc.depthEnabled && !layer.textures[view][kDepth].resource)
            return LayerResult::MissingTexture;
    }

    if (m_backend->RequiresPreparation() &&
        (render->preparedSubmit != m_submitIndex.load(std::memory_order_relaxed) || render->preparedGeneration != layer.textureGeneration))
        return LayerResult::NotPrepared;

    if (!EnsureSwapChains(session, layer, *render, viewCount))
        return LayerResult::SwapChainFailed;

    prepared = PreparedLayer{ .layer = &layer, .render = render, .viewCount = viewCount };

    return LayerResult::Submitted;
}

bool LayerStack::EnsureSwapChains(varjo_Session* session, const Layer& layer, RenderLayer& render, int32_t viewCount) {
    const int32_t usages = layer.desc.depthEnabled ? kUsageCount : 1;

    for (int32_t view = 0; view < viewCount; ++view) {
        for (int32_t usage = 0; usage < usages; ++usage) {
            const SourceTexture& texture = layer.textures[view][usage];
            SwapChain& swapChain = render.swapChains[view][usage];

            if (swapChain.handle && swapChain.desc == texture.desc)
                continue;

            if (layer.textureGeneration == render.failedGeneration)
                return false;

            FreeSwapChain(swapChain);

            if (!m_backend->CreateSwapChain(session, texture.desc, swapChain)) {
                render.failedGeneration = layer.textureGeneration;
                return false;
            }

            LogEvent("layer %d: created %s swap chain %dx%d for view %d", layer.id, usage == kColor ? "color" : "depth", swapChain.desc.width, swapChain.desc.height, view);
        }
    }

    return true;
}

void LayerStack::BuildLayer(int32_t index, const PreparedLayer& prepared, const varjo_LayerMultiProj& application) {
    const VarjoLayerDesc& desc = prepared.layer->desc;
    const RenderLayer& render = *prepared.render;
    const bool custom = desc.matrixSource == MatrixSource::Custom;

    auto& views = m_scratch.views[index];
    auto& depth = m_scratch.depth[index];
    auto& depthTestRange = m_scratch.depthTestRange[index];

    for (int32_t view = 0; view < prepared.viewCount; ++view) {
        varjo_ViewExtension* extension = nullptr;

        if (desc.depthEnabled) {
            depthTestRange[view] = varjo_ViewExtensionDepthTestRange{
                .header = varjo_ViewExtension{ .type = varjo_ViewExtensionDepthTestRangeType, .next = nullptr },
                .nearZ = desc.depthTestNearZ,
                .farZ = desc.depthTestFarZ,
            };

            depth[view] = varjo_ViewExtensionDepth{
                .header = varjo_ViewExtension{
                    .type = varjo_ViewExtensionDepthType,
                    .next = desc.depthTestRangeEnabled ? &depthTestRange[view].header : nullptr,
                },
                .minDepth = desc.minDepth,
                .maxDepth = desc.maxDepth,
                .nearZ = desc.nearZ,
                .farZ = desc.farZ,
                .viewport = ToViewport(render.swapChains[view][kDepth]),
            };

            extension = &depth[view].header;
        }

        views[view] = varjo_LayerMultiProjView{
            .extension = extension,
            .projection = custom ? ToMatrix(render.committedViews[view].projection) : application.views[view].projection,
            .view = custom ? ToMatrix(render.committedViews[view].view) : application.views[view].view,
            .viewport = ToViewport(render.swapChains[view][kColor]),
        };
    }

    const varjo_LayerFlags flags = custom ? desc.flags : desc.flags | (application.header.flags & varjo_LayerFlag_Foveated);

    m_scratch.layers[index] = varjo_LayerMultiProj{
        .header = varjo_LayerHeader{ .type = varjo_LayerMultiProjType, .flags = flags },
        .space = custom ? static_cast<varjo_Space>(desc.space) : application.space,
        .viewCount = prepared.viewCount,
        .views = views.data(),
    };
}

LayerStack::RenderLayer* LayerStack::FindOrAddRenderLayer(int32_t id) {
    RenderLayer* free = nullptr;

    for (RenderLayer& render : m_renderLayers) {
        if (render.id == id)
            return &render;

        if (render.id == 0 && !free)
            free = &render;
    }

    if (free) {
        *free = RenderLayer{};
        free->id = id;
    }

    return free;
}

void LayerStack::RemoveStaleRenderLayers(const Snapshot& snapshot) {
    for (RenderLayer& render : m_renderLayers) {
        if (render.id == 0)
            continue;

        const bool exists = std::ranges::any_of(snapshot.layers, [&render](const Layer& layer) { return layer.id == render.id; });

        if (!exists) {
            FreeSwapChains(render);
            render = RenderLayer{};
        }
    }
}

void LayerStack::FreeSwapChain(SwapChain& swapChain) {
    if (!swapChain.handle)
        return;

    if (m_backend)
        m_backend->WaitForIdle();

    if (g_varjo.IsResolved())
        g_varjo.FreeSwapChain(swapChain.handle);

    swapChain = SwapChain{};
}

void LayerStack::FreeSwapChains(RenderLayer& render) {
    for (auto& view : render.swapChains)
        for (SwapChain& swapChain : view)
            FreeSwapChain(swapChain);

    render.failedGeneration = UINT64_MAX;
}

void LayerStack::FreeAllSwapChains() {
    for (RenderLayer& render : m_renderLayers)
        FreeSwapChains(render);
}

void LayerStack::OnSessionShutDown() {
    std::lock_guard lock(m_renderMutex);

    FreeAllSwapChains();
    m_session = nullptr;

    LogEvent("session shutdown: swap chains freed after %llu frames (%llu composited)",
        static_cast<unsigned long long>(m_framesTotal.load()),
        static_cast<unsigned long long>(m_framesComposited.load()));
}

void LayerStack::RecordApplicationLayer(const varjo_LayerMultiProj& application) {
    if (m_applicationFlags.exchange(application.header.flags, std::memory_order_relaxed) != application.header.flags)
        LogEvent("application layer: flags 0x%llx, %d views", static_cast<unsigned long long>(application.header.flags), application.viewCount);

    std::lock_guard lock(m_applicationViewsMutex);
    m_applicationViewCount = std::clamp(application.viewCount, 0, kMaxViews);

    for (int32_t view = 0; view < m_applicationViewCount; ++view)
        ToView(application.views[view], m_applicationViews[view]);
}

void LayerStack::RecordFrameOutcome(FrameResult outcome) {
    const auto value = static_cast<int32_t>(outcome);

    if (m_lastOutcome.exchange(value, std::memory_order_relaxed) != value)
        LogEvent("frame outcome changed: %s", ToString(outcome));
}

void LayerStack::RecordLayerOutcome(const Layer& layer, LayerResult outcome, int32_t viewCount) {
    LayerCounters& counters = m_counters[layer.slot];
    const auto value = static_cast<int32_t>(outcome);

    if (outcome == LayerResult::Submitted)
        counters.submitted.fetch_add(1, std::memory_order_relaxed);
    else
        counters.skipped.fetch_add(1, std::memory_order_relaxed);

    counters.viewCount.store(viewCount, std::memory_order_relaxed);

    if (counters.lastResult.exchange(value, std::memory_order_relaxed) != value)
        LogEvent("layer %d: %s", layer.id, ToString(outcome));
}
