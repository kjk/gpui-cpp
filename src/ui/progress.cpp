#include "ui/progress.h"
#include "base/motion.h"
#include "gpui/paint.h"

#include <math.h>

namespace gpui {

namespace component {

// progress.rs: a value that changes takes 0.15 s to get there, and the
// indeterminate sweep is a one-second loop.
static const float kProgressMotionMs = 150.f;
static const float kProgressLoopMs = 1000.f;

Progress* Progress::New(Ctx* cx) {
    Arena* a = cx->a;
    Progress* p = ArenaNew<Progress>(a);
    p->a = a;
    p->cx = cx;
    return p;
}

Progress* Progress::Value(float v) {
    value = ProgressClampValue(v);
    return this;
}
Progress* Progress::W(float v) {
    w = v;
    return this;
}
Progress* Progress::H(float v) {
    h = v;
    return this;
}
Progress* Progress::Loading(bool v) {
    loading = v;
    return this;
}
Progress* Progress::Id(Str v) {
    id = v;
    return this;
}

El* Progress::IntoEl() {
    const Theme& th = cx->theme();
    El* indicator = gpui::ProgressIndicator::New(cx)
                        ->H(h)
                        ->Radius(h * 0.5f)
                        ->Bg(th.progress);
    if (loading) {
        // The indeterminate sweep: both edges are fractions of the track, and
        // the trailing one only starts moving halfway through the loop, so the
        // bar grows out of the left and then leaves by the right.
        float delta = MotionRepeat(cx, MotionId(StrL("progress-loading"), id),
                                   kProgressLoopMs);
        float start = EaseInOutQuad(ClampF01((delta - 0.5f) / 0.5f));
        float end = EaseInOutQuad(1.f - delta);
        indicator->Absolute()
            ->Top(0)
            ->Left(0)
            ->Right(0)
            ->LeftRel(start)
            ->RightRel(end);
    } else {
        // The indicator is a fraction of the track, the way Rust spells
        // w(relative(value / 100.)) — so it works whether the bar has a fixed
        // width or fills its parent. A value that moves takes 0.15 s to get
        // there rather than jumping.
        float v = MotionValue(cx, MotionId(StrL("progress"), id), value,
                              MotionNew(kProgressMotionMs));
        indicator->WFrac(v / 100.f);
    }
    return gpui::Progress::New(cx, StrL("progress"))
        ->W(w)
        ->Child(gpui::ProgressTrack::New(cx)
                    ->W(w)
                    ->H(h)
                    ->Radius(h * 0.5f)
                    ->Bg(RgbaOpacity(th.progress, 0.2f))
                    ->Child(indicator));
}

ProgressCircle* ProgressCircle::New(Ctx* cx) {
    Arena* a = cx->a;
    ProgressCircle* p = ArenaNew<ProgressCircle>(a);
    p->a = a;
    p->cx = cx;
    return p;
}
ProgressCircle* ProgressCircle::Value(float v) {
    value = ProgressClampValue(v);
    return this;
}
ProgressCircle* ProgressCircle::Size(float v) {
    size = v;
    return this;
}
ProgressCircle* ProgressCircle::Color(Rgba c) {
    color = c;
    hasColor = true;
    return this;
}
ProgressCircle* ProgressCircle::Label(bool v) {
    showLabel = v;
    return this;
}
ProgressCircle* ProgressCircle::Loading(bool v) {
    loading = v;
    return this;
}
ProgressCircle* ProgressCircle::Id(Str v) {
    id = v;
    return this;
}

static void PaintCircleProgress(PaintCtx* ctx, El* e, void* user) {
    auto* p = (ProgressCircle*)user;
    if (!p || !ctx->rt) {
        return;
    }
    float cx = e->x + e->w * 0.5f;
    float cy = e->y + e->h * 0.5f;
    float r = (e->w < e->h ? e->w : e->h) * 0.42f;
    if (r < 3) {
        return;
    }
    float sw = r * 0.22f;
    if (sw < 1.5f) {
        sw = 1.5f;
    }
    Rgba col = p->hasColor ? p->color : ThemeNow().foreground;
    CanvasEllipse(ctx, cx, cy, r, r, sw, RgbaOpacity(col, 0.2f));
    float v = p->value;
    if (v < 0) {
        v = 0;
    }
    if (v > 100) {
        v = 100;
    }
    if (v <= 0) {
        return;
    }
    float from = p->startValue;
    if (from < 0) {
        from = 0;
    }
    if (from > v) {
        from = v;
    }
    // render_circle(start, end): twelve o'clock is zero, and the arc runs
    // clockwise from wherever it starts.
    float start = -kPi * 0.5f + 2.f * kPi * (from / 100.f);
    float sweep = 2.f * kPi * ((v - from) / 100.f);
    Path* arc = PathNew(ctx, true);
    if (arc) {
        PathArcTo(arc, cx, cy, r, start, start + sweep, true);
        PathStroke(ctx, arc, sw, col);
        PathFree(arc);
    }
}

El* ProgressCircle::IntoEl() {
    if (loading) {
        // The same sweep as the bar's, around a circle: the leading edge runs
        // the whole loop and the trailing one chases it from halfway.
        float delta = MotionRepeat(
            cx, MotionId(StrL("progress-circle-loading"), id), kProgressLoopMs);
        value = EaseInOutQuad(delta) * 100.f;
        startValue = EaseInOutQuad(ClampF01((delta - 0.5f) / 0.5f)) * 100.f;
    } else {
        value = MotionValue(cx, MotionId(StrL("progress-circle"), id), value,
                            MotionNew(kProgressMotionMs));
        startValue = 0;
    }
    El* e = Div(a)->W(size)->H(size)->ItemsCenter()->JustifyCenter();
    e->customPaint = PaintCircleProgress;
    e->customUser = this;
    if (showLabel && size >= 28) {
        e->Child(TextEl(a, StrDup(a, fmt("%.0f%%", value)))
                     ->Font(size * 0.22f)
                     ->Fg(hasColor ? color : cx->theme().foreground));
    }
    return e;
}

} // namespace component
} // namespace gpui
