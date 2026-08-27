#ifndef GPUI_SHELL_METRICS_H_
#define GPUI_SHELL_METRICS_H_

#include "base.h"

namespace gpui {

struct RuntimeMetrics {
    uint64_t scriptRenders = 0;
    uint64_t scriptRenderNanos = 0;
    uint64_t slowestScriptRenderNanos = 0;
    uint64_t nativeNanos = 0;
    uint64_t materializations = 0;
    uint64_t materializeNanos = 0;

    uint64_t MeanScriptOnlyNanos() const;
    uint64_t MeanNativeNanos() const;
    uint64_t MeanScriptRenderNanos() const;
    uint64_t MeanMaterializeNanos() const;
    RuntimeMetrics Since(const RuntimeMetrics& earlier) const;
};

namespace shell {

struct Metrics {
    RuntimeMetrics value;
};

enum class MetricsTimerKind : uint8_t {
    ScriptRender,
    Native,
    VirtualItems,
    Materialize,
};

struct MetricsTimer {
    Metrics* metrics = nullptr;
    MetricsTimerKind kind = MetricsTimerKind::ScriptRender;
    double started = 0;
};

MetricsTimer MetricsBegin(Metrics* metrics, MetricsTimerKind kind);
void MetricsEnd(MetricsTimer* timer);
void MetricsAdd(Metrics* metrics, MetricsTimerKind kind, uint64_t nanos);
RuntimeMetrics MetricsRead(const Metrics* metrics);

} // namespace shell
} // namespace gpui
#endif // GPUI_SHELL_METRICS_H_
