/* Realtime performance HUD — crates/fps (the `gpui-fps` crate).
 *
 * Frames per second, a rolling frame time chart, and this process' CPU and
 * memory usage. Frame data comes from the window's own trace (Window::frameSeq
 * / frameTrace), so the numbers are what the runtime actually spent drawing.
 *
 * Like the Rust crate this leans on gpui only, so any example can overlay it:
 *
 *     Div(cx->a)->SizeFull()->Child(content)->Child(FpsMonitorEl(cx));
 *
 * The parent is the whole window here; the overlay places itself absolutely.
 */

#include "gpui/gpui.h"

namespace gpui {

// ─── style ────────────────────────────────────────────────────────────────

// crates/fps/src/style.rs. Internal and fixed: the palette is not
// configurable because its contrast is load bearing.
struct FpsStyle {
    Rgba background; // backdrop behind the HUD
    Rgba foreground; // primary readouts (the FPS number)
    Rgba muted;      // secondary readouts (units, labels, resource row)
    Rgba good;       // frames inside the budget
    Rgba warn;       // frames over budget but within twice of it
    Rgba bad;        // frames over twice the budget
};

// Dark HUD, legible over any window background. The backdrop is nearly opaque
// on purpose: nothing can read the pixels underneath an element, so the only
// way to stay readable everywhere is to stop the background from taking part
// in the composite.
const FpsStyle& FpsStyleDark();

// The color for a frame that took `frameSecs` against `budgetSecs`.
Rgba FpsLevelColor(const FpsStyle& style, float frameSecs, float budgetSecs);

// ─── sampler ──────────────────────────────────────────────────────────────

enum {
    // DEFAULT_CAPACITY: frames the chart keeps.
    kFpsCapacity = 120,
    // Arrivals inside the one second FPS window. An uncapped renderer can beat
    // the sample capacity, so this is sized well past it.
    kFpsArrivals = 512,
};

// crates/fps/src/sampler.rs. Drains the window's frame trace and keeps the
// last `capacity` draw times plus the arrivals inside the FPS window.
struct FrameSampler {
    float draws[kFpsCapacity] = {}; // seconds, oldest first
    int n = 0;
    int capacity = kFpsCapacity;
    double arrivals[kFpsArrivals] = {};
    int nArrivals = 0;
    uint64_t cursor = 0; // FrameTimingCollector position
};

// Drains the frames drawn since the previous call. Call once per rendered
// frame.
void FrameSamplerTick(FrameSampler* s, Window* win);
// ingest(): the half of the tick that is not the window. Takes the draw times
// of the frames that arrived and the moment they arrived at, which is what
// makes the rolling window testable without a window to drive it. Rust filters
// the process-wide frame trace by window id here; ours is already per-window.
void FrameSamplerIngest(FrameSampler* s, const float* drawSecs, int n,
                        double now);
void FrameSamplerSetCapacity(FrameSampler* s, int capacity);
// Frames per second over the arrivals still inside the one second window. `n`
// frames span `n - 1` intervals, so the rate comes from the elapsed span
// rather than the raw count; that keeps it correct before the window fills.
float FrameSamplerFps(const FrameSampler* s);
float FrameSamplerMeanDraw(const FrameSampler* s);
float FrameSamplerPeakDraw(const FrameSampler* s);
// Share of the retained frames that overran `budgetSecs`, in 0..1.
float FrameSamplerOverBudget(const FrameSampler* s, float budgetSecs);

// A sample of this process' resource usage.
struct ResourceSample {
    // Normalized so 100 means every logical core is saturated.
    float cpuPercent = 0;
    uint64_t memoryBytes = 0;
};

// Rust probes this on a background thread through sysinfo, because refreshing
// walks the process table. Asking Windows about one known process is two cheap
// calls, so this samples inline on the render thread instead of growing an
// executor for it. Returns false until it has a delta to divide by.
struct ResourceProbe {
    uint64_t prevCpu100ns = 0;
    double prevAt = 0;
    float cores = 1;
    bool primed = false;
};

bool ResourceProbeSample(ResourceProbe* probe, ResourceSample* out);

// ─── monitor ──────────────────────────────────────────────────────────────

// The numbers as last published to the screen.
struct FpsReadout {
    float fps = 0;
    float frameMillis = 0;
    float droppedPercent = 0;
};

// crates/fps/src/monitor.rs. A view rather than a stateless component, so the
// click that collapses it has an entity to run against.
struct FpsMonitor {
    FrameSampler sampler;
    FpsReadout readout;
    double readoutAt = -1;
    // One 60Hz frame, the budget a frame is judged against. Set it to 1/144
    // on a high refresh rate display.
    float frameBudget = 1.f / 60.f;
    // Keep asking for frames, so the readout behaves like an in-game counter:
    // the rate the application *can* sustain, not the rate it happens to draw
    // at. Turn it off to measure the real workload.
    bool continuous = true;
    bool showResources = true;
    float resourceInterval = 0.5f;
    ResourceProbe probe;
    ResourceSample resources;
    bool hasResources = false;
    double resourcesAt = -1;
    bool compact = false;
    // Upper bound of the chart's y axis, in seconds.
    float axisMax = (1.f / 60.f) * 2.f;

    static El* Render(FpsMonitor* self, Ctx* cx);
    static void OnToggleCompact(FpsMonitor* self, Ctx* cx, const ClickEvent*);
};

// Where in the parent the HUD sits.
enum class FpsAnchor : uint8_t {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    TopCenter,
    BottomCenter,
    LeftCenter,
    RightCenter,
};

// crates/fps/src/overlay.rs. Pins a monitor to an edge or corner of its
// parent, the way a game overlays its frame counter.
El* FpsOverlayEl(Ctx* cx, Entity<FpsMonitor> monitor, FpsAnchor anchor);

// gpui_fps::fps_monitor: the HUD pinned to the top right, with the monitor
// created on first use and reused afterwards, one per window. Render it only
// when it should be visible.
El* FpsMonitorEl(Ctx* cx);

} // namespace gpui
