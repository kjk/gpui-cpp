#include "ui/progress.h"
#include "gpui/paint.h"

#include <math.h>

namespace gpui {

namespace component {

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

El* Progress::IntoEl() {
    const Theme& th = cx->theme();
    return gpui::Progress::New(cx, StrL("progress"))
        ->W(w)
        ->Child(gpui::ProgressTrack::New(cx)
                    ->W(w)
                    ->H(h)
                    ->Radius(h * 0.5f)
                    ->Bg(RgbaOpacity(th.progress, 0.2f))
                    // The indicator is a fraction of the track, the way Rust
                    // spells w(relative(value / 100.)) — so it works whether
                    // the bar has a fixed width or fills its parent.
                    ->Child(gpui::ProgressIndicator::New(cx)
                                ->WFrac(value / 100.f)
                                ->H(h)
                                ->Radius(h * 0.5f)
                                ->Bg(th.progress)));
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
    float start = -kPi * 0.5f;
    float sweep = 2.f * kPi * (v / 100.f);
    Path* arc = PathNew(ctx, true);
    if (arc) {
        PathArcTo(arc, cx, cy, r, start, start + sweep, true);
        PathStroke(ctx, arc, sw, col);
        PathFree(arc);
    }
}

El* ProgressCircle::IntoEl() {
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
