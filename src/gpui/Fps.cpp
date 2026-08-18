#include "gpui/Fps.h"
#include "gpui/Paint.h"

namespace gpui {

// ─── style (crates/fps/src/style.rs) ──────────────────────────────────────

const FpsStyle& FpsStyleDark() {
    // The trace colors lean bright and saturated so the chart reads like a
    // vitals monitor against the dark backdrop.
    static FpsStyle style = {
        RgbaHsla(0.f, 0.f, 0.04f, 0.92f),   // background
        RgbaHsla(0.f, 0.f, 0.98f, 1.f),     // foreground
        RgbaHsla(0.f, 0.f, 0.62f, 1.f),     // muted
        RgbaHsla(0.41f, 0.95f, 0.56f, 1.f), // good
        RgbaHsla(0.11f, 0.95f, 0.6f, 1.f),  // warn
        RgbaHsla(0.99f, 0.9f, 0.62f, 1.f),  // bad
    };
    return style;
}

Rgba FpsLevelColor(const FpsStyle& style, float frameSecs, float budgetSecs) {
    if (frameSecs <= budgetSecs) {
        return style.good;
    }
    if (frameSecs <= budgetSecs * 2.f) {
        return style.warn;
    }
    return style.bad;
}

// ─── sampler (crates/fps/src/sampler.rs) ──────────────────────────────────

// Frames older than this stop contributing to the FPS readout.
static const double kFpsWindow = 1.0;

void FrameSamplerSetCapacity(FrameSampler* s, int capacity) {
    if (capacity < 1) {
        capacity = 1;
    }
    if (capacity > kFpsCapacity) {
        capacity = kFpsCapacity;
    }
    s->capacity = capacity;
    if (s->n > capacity) {
        int drop = s->n - capacity;
        memmove(s->draws, s->draws + drop, sizeof(float) * (size_t)capacity);
        s->n = capacity;
    }
}

static void FrameSamplerPush(FrameSampler* s, float drawSecs, double now) {
    if (s->n == s->capacity) {
        memmove(s->draws, s->draws + 1, sizeof(float) * (size_t)(s->n - 1));
        s->n--;
    }
    s->draws[s->n++] = drawSecs;

    if (s->nArrivals == kFpsArrivals) {
        memmove(s->arrivals, s->arrivals + 1,
                sizeof(double) * (size_t)(s->nArrivals - 1));
        s->nArrivals--;
    }
    s->arrivals[s->nArrivals++] = now;
}

void FrameSamplerIngest(FrameSampler* s, const float* drawSecs, int n,
                        double now) {
    if (!s) {
        return;
    }
    if (s->capacity < 1 || s->capacity > kFpsCapacity) {
        s->capacity = kFpsCapacity;
    }
    for (int i = 0; i < n; i++) {
        FrameSamplerPush(s, drawSecs[i], now);
    }

    int drop = 0;
    while (drop < s->nArrivals && now - s->arrivals[drop] > kFpsWindow) {
        drop++;
    }
    if (drop > 0) {
        memmove(s->arrivals, s->arrivals + drop,
                sizeof(double) * (size_t)(s->nArrivals - drop));
        s->nArrivals -= drop;
    }
}

void FrameSamplerTick(FrameSampler* s, Window* win) {
    if (!s || !win) {
        return;
    }
    FrameTiming timings[kFrameTraceCap];
    int n = WindowCollectFrames(win, &s->cursor, timings, kFrameTraceCap);
    float draws[kFrameTraceCap];
    for (int i = 0; i < n; i++) {
        draws[i] = timings[i].drawSecs;
    }
    FrameSamplerIngest(s, draws, n, TimeNow());
}

float FrameSamplerFps(const FrameSampler* s) {
    if (s->nArrivals < 2) {
        return 0;
    }
    double span = s->arrivals[s->nArrivals - 1] - s->arrivals[0];
    if (span <= 0) {
        return 0;
    }
    return (float)((double)(s->nArrivals - 1) / span);
}

float FrameSamplerMeanDraw(const FrameSampler* s) {
    if (s->n <= 0) {
        return 0;
    }
    double total = 0;
    for (int i = 0; i < s->n; i++) {
        total += s->draws[i];
    }
    return (float)(total / (double)s->n);
}

float FrameSamplerPeakDraw(const FrameSampler* s) {
    float peak = 0;
    for (int i = 0; i < s->n; i++) {
        if (s->draws[i] > peak) {
            peak = s->draws[i];
        }
    }
    return peak;
}

float FrameSamplerOverBudget(const FrameSampler* s, float budgetSecs) {
    if (s->n <= 0) {
        return 0;
    }
    int over = 0;
    for (int i = 0; i < s->n; i++) {
        if (s->draws[i] > budgetSecs) {
            over++;
        }
    }
    return (float)over / (float)s->n;
}

bool ResourceProbeSample(ResourceProbe* probe, ResourceSample* out) {
    if (!probe || !out) {
        return false;
    }
    if (!probe->primed) {
        probe->cores = (float)PlatCoreCount();
    }
    uint64_t cpu100ns = 0;
    uint64_t memBytes = 0;
    if (!PlatSelfUsage(&cpu100ns, &memBytes)) {
        return false;
    }
    double now = TimeNow();

    // The first sample only establishes the baseline; CPU is a delta against
    // the previous one and reads zero until there is one.
    bool primed = probe->primed;
    double elapsed = now - probe->prevAt;
    uint64_t delta = cpu100ns - probe->prevCpu100ns;
    probe->prevCpu100ns = cpu100ns;
    probe->prevAt = now;
    probe->primed = true;
    if (!primed || elapsed <= 0) {
        return false;
    }

    // 100ns ticks of CPU over 100ns ticks of wall clock across every core.
    float cpu = (float)((double)delta / (elapsed * 1e7 * probe->cores) * 100.);
    out->cpuPercent = cpu > 100.f ? 100.f : cpu;
    out->memoryBytes = memBytes;
    return true;
}

// ─── monitor (crates/fps/src/monitor.rs) ──────────────────────────────────

// How fast the chart's y axis relaxes back down after a spike. Growth is
// immediate so a slow frame is never clipped, while the decay is gradual so
// the bars don't visibly rescale every frame.
static const float kAxisDecay = 0.04f;

// A fixed width keeps every row flush with the chart and stops the HUD from
// resizing as the readings gain or lose digits. Collapsed, the HUD hugs its
// text instead and only the figure gets a fixed box.
static const float kHudWidth = 172.f;
static const float kCompactFigureWidth = 25.f;

// Size of every label and reading. Collapsed, the figure uses it too.
static const float kTextSize = 10.f;

// The trace sits behind the headline, so it is dimmed enough to stay out of
// the figure's way while still showing its shape and color.
static const float kTraceOpacity = 0.35f;

// Tall enough to give the trace room to show its shape around the figure.
static const float kHeadlineHeight = 35.f;

// The headline figure, in a box wide enough for four digits: an uncapped
// frame rate on a small window reaches four figures.
static const float kFigureSize = 28.f;
static const float kFigureWidth = 70.f;

// Width of the `FPS` unit, and of the empty box mirroring it on the other side
// of the figure so the figure lands on the HUD's true center.
static const float kUnitWidth = 22.f;

// How often the numbers are recomputed. The trace keeps up with every frame,
// but the readings do not: recomputed per frame they flicker through digits
// too fast to read.
static const double kReadoutInterval = 0.5;

// Fraction of the target frame rate that still counts as meeting it. Vsync and
// the sampling window each cost a frame or so a second, so a 60Hz display that
// is keeping up perfectly reports 58 to 60, never a flat 60.
static const float kFpsTolerance = 0.95f;

// Distance from the edges the overlay is pinned to.
static const float kOverlayMargin = 12.f;

// Grades the frame rate against the rate the budget implies.
//
// This deliberately does not compare 1/fps against the budget the way the
// per-frame trace does. Under vsync the measured rate lands just under the
// refresh rate essentially always, so an exact comparison would paint a
// perfectly healthy application as over budget.
static Rgba FpsColor(float fps, float budgetSecs, const FpsStyle& style) {
    if (fps <= 0) {
        return style.muted;
    }
    float target = 1.f / budgetSecs;
    if (fps >= target * kFpsTolerance) {
        return style.good;
    }
    if (fps >= target * 0.5f) {
        return style.warn;
    }
    return style.bad;
}

static TempStr FpsFormatBytes(uint64_t bytes) {
    const double kMib = 1024. * 1024.;
    const double kGib = kMib * 1024.;
    double v = (double)bytes;
    if (v >= kGib) {
        return fmt("%.2f GB", v / kGib);
    }
    return fmt("%.0f MB", v / kMib);
}

// Republishes the readings if kReadoutInterval has passed.
static void UpdateReadout(FpsMonitor* self) {
    double now = TimeNow();
    if (self->readoutAt >= 0 && now - self->readoutAt < kReadoutInterval) {
        return;
    }
    self->readout.fps = FrameSamplerFps(&self->sampler);
    // The mean over the interval rather than the latest frame, which at this
    // cadence would be an arbitrary sample.
    self->readout.frameMillis = FrameSamplerMeanDraw(&self->sampler) * 1000.f;
    self->readout.droppedPercent =
        FrameSamplerOverBudget(&self->sampler, self->frameBudget) * 100.f;
    self->readoutAt = now;
}

// Grows immediately to fit the slowest retained frame and decays back slowly,
// so a single spike doesn't make the whole chart jump.
static void UpdateAxis(FpsMonitor* self) {
    float floorSecs = self->frameBudget * 2.f;
    float target = FrameSamplerPeakDraw(&self->sampler);
    if (target < floorSecs) {
        target = floorSecs;
    }
    self->axisMax = target > self->axisMax
                        ? target
                        : self->axisMax + (target - self->axisMax) * kAxisDecay;
}

static void UpdateResources(FpsMonitor* self) {
    if (!self->showResources) {
        return;
    }
    double now = TimeNow();
    if (self->resourcesAt >= 0 &&
        now - self->resourcesAt < (double)self->resourceInterval) {
        return;
    }
    self->resourcesAt = now;
    ResourceSample sample;
    if (ResourceProbeSample(&self->probe, &sample)) {
        self->resources = sample;
        self->hasResources = true;
    }
}

static bool SameRgba(Rgba a, Rgba b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

// The frame time trace, drawn behind the readings so it fills the HUD instead
// of taking a band of its own. Drawn as runs of equal color rather than one
// segment per frame: in the common case where nothing is dropped the whole
// chart collapses into a single run.
static void PaintFpsTrace(PaintCtx* ctx, El* e, void* user) {
    auto* self = (FpsMonitor*)user;
    const FrameSampler* s = &self->sampler;
    if (!ctx->rt || s->n < 2 || e->w <= 0 || e->h <= 0) {
        return;
    }
    const FpsStyle& style = FpsStyleDark();
    float axisMax = self->axisMax > 1e-6f ? self->axisMax : 1e-6f;
    float slot = e->w / (float)s->capacity;
    // Fewer samples than the capacity means the chart is still filling up;
    // keep the newest frame pinned to the right edge so the history scrolls
    // instead of stretching.
    int leading = s->capacity - s->n;
    if (leading < 0) {
        leading = 0;
    }

    float px[kFpsCapacity];
    float py[kFpsCapacity];
    Rgba colors[kFpsCapacity];
    for (int i = 0; i < s->n; i++) {
        float secs = s->draws[i];
        float ratio = secs / axisMax;
        if (ratio < 0) {
            ratio = 0;
        }
        if (ratio > 1) {
            ratio = 1;
        }
        px[i] = e->x + slot * (float)(leading + i) + slot * 0.5f;
        py[i] = e->y + e->h * (1.f - ratio);
        colors[i] = RgbaOpacity(FpsLevelColor(style, secs, self->frameBudget),
                                kTraceOpacity);
    }

    int start = 0;
    while (start + 1 < s->n) {
        // A segment is as slow as the frame it ends on, so the color of the
        // later point decides the run.
        Rgba color = colors[start + 1];
        int end = start + 1;
        while (end < s->n && SameRgba(colors[end], color)) {
            CanvasLine(ctx, px[end - 1], py[end - 1], px[end], py[end], 1.f,
                       color);
            end++;
        }
        // Share the boundary point with the next run so the line stays
        // connected across a color change.
        start = end - 1;
    }
}

// A `LABEL value` pair kept together, for rows that carry more than one
// reading. The label stays muted so it reads as a caption, not as data.
static El* FpsPair(Ctx* cx, Str label, Str value, const FpsStyle& style) {
    return Div(cx->a)
        ->FlexRow()
        ->Gap(4)
        ->Child(TextEl(cx->a, label)->Fg(style.muted))
        ->Child(TextEl(cx->a, value)->Fg(style.foreground));
}

// One `LABEL … value` row. The value is right aligned against the HUD's inner
// edge, so in a monospace font every row's digits line up in a column and
// nothing shifts as the readings change width.
static El* FpsReading(Ctx* cx, Str label, Str value, Rgba valueColor,
                      const FpsStyle& style) {
    return Div(cx->a)
        ->FlexRow()
        ->W(kFill)
        ->JustifyBetween()
        ->Gap(8)
        ->PadY(1)
        ->Child(TextEl(cx->a, label)->Fg(style.muted))
        ->Child(TextEl(cx->a, value)->Fg(valueColor));
}

// The headline reading, with the frame time trace painted behind it.
//
// The trace lives in this row rather than spanning the whole HUD because this
// is its emptiest part — the figure is centered and short, leaving both flanks
// open — so the trace stays readable instead of being cut up by the denser
// rows below.
static El* FpsHeadline(Ctx* cx, FpsMonitor* self, float fps, Rgba color,
                       const FpsStyle& style) {
    El* trace = Div(cx->a)->Absolute()->Top(0)->Left(0)->SizeFull();
    trace->customPaint = PaintFpsTrace;
    trace->customUser = self;

    // The figure is centered in a fixed box so neither the unit nor the group
    // shifts as the count gains or loses a digit; the two share a bottom edge.
    // The box is the figure's own size because the headline sets
    // line_height(relative(1.)) on it, tighter than the inherited phi.
    El* figure = Div(cx->a)
                     ->W(kFigureWidth)
                     ->H(kFigureSize)
                     ->FlexRow()
                     ->ItemsCenter()
                     ->JustifyCenter()
                     ->Child(TextEl(cx->a, fmt("%.0f", fps))
                                 ->Font(kFigureSize)
                                 ->LineHeight(1.f)
                                 ->Fg(color));

    return Div(cx->a)
        ->ClipY()
        ->W(kFill)
        ->H(kHeadlineHeight)
        ->Child(trace)
        ->Child(Div(cx->a)
                    ->FlexRow()
                    ->SizeFull()
                    ->ItemsEnd()
                    ->JustifyCenter()
                    ->Gap(4)
                    // An empty box matching the unit on the right. Without it
                    // the unit's own width pushes the figure off center by
                    // half of it, which reads as misalignment.
                    ->Child(Div(cx->a)->W(kUnitWidth)->H(kTextSize))
                    ->Child(figure)
                    ->Child(Div(cx->a)
                                ->W(kUnitWidth)
                                ->Child(TextEl(cx->a, StrL("FPS"))
                                            ->Fg(style.muted))));
}

void FpsMonitor::OnToggleCompact(FpsMonitor* self, Ctx* cx, const ClickEvent*) {
    self->compact = !self->compact;
    Notify(cx);
}

El* FpsMonitor::Render(FpsMonitor* self, Ctx* cx) {
    FrameSamplerTick(&self->sampler, cx->win);
    UpdateReadout(self);
    UpdateAxis(self);
    UpdateResources(self);
    // The HUD keeps the window drawing back to back. GPUI spells this
    // window.request_animation_frame() once per render.
    if (self->continuous && cx->win && !cx->win->anim) {
        AppRequestAnim(cx->win, true);
    }

    const FpsStyle& style = FpsStyleDark();
    FpsReadout r = self->readout;
    Rgba fpsColor = FpsColor(r.fps, self->frameBudget, style);

    El* hud = Div(cx->a)
                  ->Click(HashClickId(StrL("gpui-fps-hud")))
                  ->FlexRow()
                  ->Bg(style.background)
                  ->Mono()
                  ->Font(kTextSize)
                  ->OnClick(Listen(cx, &FpsMonitor::OnToggleCompact));

    if (self->compact) {
        // Collapsed, the HUD is one small tag: the figure drops to the same
        // size as its unit, the box shrinks to the text, and everything else
        // is dropped, so it sits over the interface without competing with it.
        return hud->ItemsCenter()
            ->Gap(4)
            ->PadX(6)
            ->PadY(2)
            ->Radius(3)
            ->Child(
                Div(cx->a)
                    ->W(kCompactFigureWidth)
                    ->FlexRow()
                    ->JustifyEnd()
                    ->Child(TextEl(cx->a, fmt("%.0f", r.fps))->Fg(fpsColor)))
            ->Child(TextEl(cx->a, StrL("FPS"))->Fg(style.muted));
    }

    hud->FlexCol()
        ->W(kHudWidth)
        ->PadX(8)
        ->PadY(6)
        ->Radius(4)
        ->Child(FpsHeadline(cx, self, r.fps, fpsColor, style))
        ->Child(FpsReading(cx, StrL("FRAME"), fmt("%.1f ms", r.frameMillis),
                           style.foreground, style))
        ->Child(FpsReading(
            cx, StrL("DROP"), fmt("%.1f%%", r.droppedPercent),
            FpsLevelColor(style, r.droppedPercent > 0 ? 1.f : 0.f, 0.5f),
            style));
    if (self->showResources && self->hasResources) {
        // CPU and memory share a row: both are coarse background samples,
        // unlike the per-frame numbers.
        hud->Child(
            Div(cx->a)
                ->FlexRow()
                ->W(kFill)
                ->JustifyBetween()
                ->Gap(8)
                ->PadY(1)
                ->Child(FpsPair(cx, StrL("CPU"),
                                fmt("%.1f%%", self->resources.cpuPercent),
                                style))
                ->Child(FpsPair(cx, StrL("MEM"),
                                FpsFormatBytes(self->resources.memoryBytes),
                                style)));
    }
    return hud;
}

// ─── overlay (crates/fps/src/overlay.rs) ──────────────────────────────────

El* FpsOverlayEl(Ctx* cx, Entity<FpsMonitor> monitor, FpsAnchor anchor) {
    El* hud = EntityRender(cx->app, cx->win, cx->a, monitor.id);
    if (!hud) {
        return Div(cx->a);
    }
    // Corners are placed by their own two offsets so the overlay stays the
    // size of the HUD. The centered anchors need a strip to center within, but
    // it is only stretched along the one axis that needs it, keeping the area
    // laid over the content as small as possible.
    El* box = Div(cx->a)->Absolute()->FlexRow();
    float m = kOverlayMargin;
    switch (anchor) {
        case FpsAnchor::TopLeft:
            box->Top(m)->Left(m);
            break;
        case FpsAnchor::TopRight:
            box->Top(m)->Right(m);
            break;
        case FpsAnchor::BottomLeft:
            box->Bottom(m)->Left(m);
            break;
        case FpsAnchor::BottomRight:
            box->Bottom(m)->Right(m);
            break;
        case FpsAnchor::TopCenter:
            box->Top(m)->Left(0)->W(kFill)->JustifyCenter();
            break;
        case FpsAnchor::BottomCenter:
            box->Bottom(m)->Left(0)->W(kFill)->JustifyCenter();
            break;
        case FpsAnchor::LeftCenter:
            box->Left(m)->Top(0)->H(kFill)->ItemsCenter();
            break;
        case FpsAnchor::RightCenter:
            box->Right(m)->Top(0)->H(kFill)->ItemsCenter();
            break;
    }
    return box->Child(hud);
}

El* FpsMonitorEl(Ctx* cx) {
    // The monitor is created on first use and reused afterwards, one per
    // window, so this can be called straight from Render every frame. Rust
    // keeps the same map as a global keyed by WindowId.
    auto* slot = KeyedState<Entity<FpsMonitor>>(
        cx, (uint32_t)HashClickId(StrL("gpui-fps-monitor")));
    if (!slot) {
        return Div(cx->a);
    }
    if (!slot->IsValid()) {
        *slot = EntityNew<FpsMonitor>(cx);
    }
    return FpsOverlayEl(cx, *slot, FpsAnchor::TopRight);
}

} // namespace gpui
