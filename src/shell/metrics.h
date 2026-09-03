#ifndef GPUI_SHELL_METRICS_H_
#define GPUI_SHELL_METRICS_H_

#include "base.h"

namespace gpui {

struct RuntimeMetrics {
    uint64_t scriptRenders = 0;
    uint64_t scriptRenderNanos = 0;
    uint64_t slowestScriptRenderNanos = 0;
    uint64_t nativeNanos = 0;
    // Calls into JavaScript made from GPUI's frame path: a window of a
    // virtualized list's items, or one piece of a dock's chrome.
    uint64_t frameScriptCalls = 0;
    uint64_t materializations = 0;
    uint64_t materializeNanos = 0;
    // Rebuilds that produced the same *shape* as the description they
    // replaced — the same components, the same builder methods, the same tree
    // — differing only in the values inside it. A view's first build has no
    // predecessor and is counted in neither this nor `structureChanges`, so
    // the two sum to the rebuilds that had one rather than to `scriptRenders`.
    uint64_t structureRepeats = 0;
    // Rebuilds whose shape differed from the description they replaced: a
    // branch taken differently, a row added, a style method that was not
    // called last time.
    uint64_t structureChanges = 0;

    // What fraction of rebuilds with a predecessor repeated its shape, in the
    // range 0..1. False when no rebuild has had one yet.
    bool StructureRepeatRate(double* out) const;
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
    // One script call GPUI makes from inside a frame. Added to the
    // materialize total without moving the materialize count: the count is
    // materializations *of a snapshot*, and these are not — they happen
    // several times inside a single frame, from inside the layout pass. The
    // time belongs there all the same, since it is spent on the frame's
    // budget.
    FrameScript,
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

// Records that a rebuild either repeated the shape of the description it
// replaced or did not. Called only when there *was* a predecessor: a view's
// first build is not a data point about whether structure repeats.
void MetricsRecordStructure(Metrics* metrics, bool repeated);

} // namespace shell
} // namespace gpui
#endif // GPUI_SHELL_METRICS_H_
