#include "shell/metrics.h"

#include "gpui/gpui.h"

namespace gpui {

static uint64_t MeanNanos(uint64_t total, uint64_t count) {
    if (count == 0 || count > UINT32_MAX) {
        return 0;
    }
    return total / count;
}

bool RuntimeMetrics::StructureRepeatRate(double* out) const {
    uint64_t compared = structureRepeats + structureChanges;
    if (compared == 0) return false;
    if (out) *out = (double)structureRepeats / (double)compared;
    return true;
}

uint64_t RuntimeMetrics::MeanScriptOnlyNanos() const {
    uint64_t scriptOnly =
        scriptRenderNanos > nativeNanos ? scriptRenderNanos - nativeNanos : 0;
    return MeanNanos(scriptOnly, scriptRenders);
}

uint64_t RuntimeMetrics::MeanNativeNanos() const {
    return MeanNanos(nativeNanos, scriptRenders);
}
uint64_t RuntimeMetrics::MeanScriptRenderNanos() const {
    return MeanNanos(scriptRenderNanos, scriptRenders);
}
uint64_t RuntimeMetrics::MeanMaterializeNanos() const {
    return MeanNanos(materializeNanos, materializations);
}

static uint64_t SaturatingSub(uint64_t value, uint64_t earlier) {
    return value > earlier ? value - earlier : 0;
}

RuntimeMetrics RuntimeMetrics::Since(const RuntimeMetrics& earlier) const {
    RuntimeMetrics out;
    out.scriptRenders = SaturatingSub(scriptRenders, earlier.scriptRenders);
    out.scriptRenderNanos =
        SaturatingSub(scriptRenderNanos, earlier.scriptRenderNanos);
    out.slowestScriptRenderNanos = slowestScriptRenderNanos;
    out.nativeNanos = SaturatingSub(nativeNanos, earlier.nativeNanos);
    out.frameScriptCalls =
        SaturatingSub(frameScriptCalls, earlier.frameScriptCalls);
    out.materializations =
        SaturatingSub(materializations, earlier.materializations);
    out.materializeNanos =
        SaturatingSub(materializeNanos, earlier.materializeNanos);
    out.structureRepeats =
        SaturatingSub(structureRepeats, earlier.structureRepeats);
    out.structureChanges =
        SaturatingSub(structureChanges, earlier.structureChanges);
    return out;
}

namespace shell {

static uint64_t SaturatingAdd(uint64_t left, uint64_t right) {
    return UINT64_MAX - left < right ? UINT64_MAX : left + right;
}

void MetricsAdd(Metrics* metrics, MetricsTimerKind kind, uint64_t nanos) {
    if (!metrics) {
        return;
    }
    RuntimeMetrics& value = metrics->value;
    switch (kind) {
        case MetricsTimerKind::ScriptRender:
            value.scriptRenders = SaturatingAdd(value.scriptRenders, 1);
            value.scriptRenderNanos =
                SaturatingAdd(value.scriptRenderNanos, nanos);
            value.slowestScriptRenderNanos =
                std::max(value.slowestScriptRenderNanos, nanos);
            break;
        case MetricsTimerKind::Native:
            value.nativeNanos = SaturatingAdd(value.nativeNanos, nanos);
            break;
        case MetricsTimerKind::FrameScript:
            value.frameScriptCalls = SaturatingAdd(value.frameScriptCalls, 1);
            value.materializeNanos =
                SaturatingAdd(value.materializeNanos, nanos);
            break;
        case MetricsTimerKind::Materialize:
            value.materializations = SaturatingAdd(value.materializations, 1);
            value.materializeNanos =
                SaturatingAdd(value.materializeNanos, nanos);
            break;
    }
}

MetricsTimer MetricsBegin(Metrics* metrics, MetricsTimerKind kind) {
    return MetricsTimer{metrics, kind, TimeNow()};
}

void MetricsEnd(MetricsTimer* timer) {
    if (!timer || !timer->metrics) {
        return;
    }
    double elapsed = TimeNow() - timer->started;
    uint64_t nanos = elapsed <= 0 ? 0
                     : elapsed >= (double)UINT64_MAX / 1e9
                         ? UINT64_MAX
                         : (uint64_t)(elapsed * 1e9);
    MetricsAdd(timer->metrics, timer->kind, nanos);
    timer->metrics = nullptr;
}

void MetricsRecordStructure(Metrics* metrics, bool repeated) {
    if (!metrics) return;
    uint64_t& counter = repeated ? metrics->value.structureRepeats
                                 : metrics->value.structureChanges;
    counter = SaturatingAdd(counter, 1);
}

RuntimeMetrics MetricsRead(const Metrics* metrics) {
    return metrics ? metrics->value : RuntimeMetrics{};
}

} // namespace shell
} // namespace gpui
