#include "component/Progress.h"

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
    value = v;
    if (value < 0) {
        value = 0;
    }
    if (value > 100) {
        value = 100;
    }
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
    const Theme& th = ThemeNow();
    return gpui::Progress::New(cx, StrL("progress"))
        ->W(w)
        ->Child(gpui::ProgressTrack::New(cx)
                    ->W(w)
                    ->H(h)
                    ->Radius(h * 0.5f)
                    ->Bg(RgbaOpacity(th.progress, 0.2f))
                    ->Child(gpui::ProgressIndicator::New(cx)
                                ->W(w * (value / 100.f))
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
    value = v;
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
    if (!p || !ctx->rt || !ctx->brush) {
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
    ctx->brush->SetColor(RgbaToD2D(RgbaOpacity(col, 0.2f)));
    ctx->rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), ctx->brush,
                         sw);
    float v = p->value;
    if (v < 0) {
        v = 0;
    }
    if (v > 100) {
        v = 100;
    }
    if (v <= 0 || !ctx->d2d) {
        return;
    }
    const float kPi = 3.14159265f;
    float start = -kPi * 0.5f;
    float sweep = 2.f * kPi * (v / 100.f);
    ID2D1PathGeometry* geo = nullptr;
    if (FAILED(ctx->d2d->CreatePathGeometry(&geo)) || !geo) {
        return;
    }
    ID2D1GeometrySink* sink = nullptr;
    if (SUCCEEDED(geo->Open(&sink)) && sink) {
        float x0 = cx + r * cosf(start);
        float y0 = cy + r * sinf(start);
        float x1 = cx + r * cosf(start + sweep);
        float y1 = cy + r * sinf(start + sweep);
        sink->BeginFigure(D2D1::Point2F(x0, y0), D2D1_FIGURE_BEGIN_HOLLOW);
        D2D1_ARC_SEGMENT arc = {};
        arc.point = D2D1::Point2F(x1, y1);
        arc.size = D2D1::SizeF(r, r);
        arc.rotationAngle = 0;
        arc.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
        arc.arcSize = sweep > kPi ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL;
        sink->AddArc(arc);
        sink->EndFigure(D2D1_FIGURE_END_OPEN);
        sink->Close();
        sink->Release();
    }
    ctx->brush->SetColor(RgbaToD2D(col));
    ctx->rt->DrawGeometry(geo, ctx->brush, sw);
    geo->Release();
}

El* ProgressCircle::IntoEl() {
    El* e = Div(a)->W(size)->H(size)->ItemsCenter()->JustifyCenter();
    e->customPaint = PaintCircleProgress;
    e->customUser = this;
    if (showLabel && size >= 28) {
        e->Child(TextEl(a, StrDup(a, fmt("%.0f%%", value)))
                     ->Font(size * 0.22f)
                     ->Fg(hasColor ? color : ThemeNow().foreground));
    }
    return e;
}

} // namespace component
} // namespace gpui
