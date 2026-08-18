#include "component/Chart.h"

namespace gpui {

namespace component {

AreaChart* AreaChart::New(Ctx* cx, const float* ys, int n) {
    Arena* a = cx->a;
    AreaChart* c = ArenaNew<AreaChart>(a);
    c->a = a;
    c->cx = cx;
    c->ys = ys;
    c->n = n;
    c->stroke = cx->theme().blue;
    c->fill = RgbaOpacity(cx->theme().blue, 0.25f);
    return c;
}
AreaChart* AreaChart::Stroke(Rgba c) {
    stroke = c;
    return this;
}
AreaChart* AreaChart::Fill(Rgba c) {
    fill = c;
    return this;
}

AreaChart* AreaChart::Labels(const char* const* l) {
    labels = l;
    return this;
}
AreaChart* AreaChart::TickMargin(int t) {
    tickMargin = t;
    return this;
}
AreaChart* AreaChart::Overlay(bool v) {
    overlay = v;
    return this;
}

El* AreaChart::IntoEl() {
    El* e =
        ChartEl(a, ys, n, stroke, fill, RgbaOpacity(fill, 0.0f), tickMargin);
    e->chart.labels = labels;
    e->chart.overlay = overlay;
    return e;
}

PieChart* PieChart::New(Ctx* cx) {
    Arena* a = cx->a;
    PieChart* p = ArenaNew<PieChart>(a);
    p->a = a;
    p->cx = cx;
    return p;
}
PieChart* PieChart::Slice(float value, Rgba color, float outerInset) {
    if (n < 12) {
        slices[n].value = value;
        slices[n].color = color;
        slices[n].outerInset = outerInset;
        n++;
    }
    return this;
}
PieChart* PieChart::OuterRadius(float r) {
    outerRadius = r;
    return this;
}
PieChart* PieChart::InnerRadius(float r) {
    innerRadius = r;
    return this;
}
PieChart* PieChart::PadAngle(float radians) {
    padAngle = radians;
    return this;
}

static void PaintPie(PaintCtx* ctx, El* e, void* user) {
    auto* p = (PieChart*)user;
    if (!p || !ctx->rt || !ctx->brush || !ctx->d2d || p->n == 0) {
        return;
    }
    float cx = e->x + e->w * 0.5f;
    float cy = e->y + e->h * 0.5f;
    float total = 0;
    for (int i = 0; i < p->n; i++) {
        total += p->slices[i].value;
    }
    if (total <= 0) {
        return;
    }
    const float kPi = 3.14159265f;
    float angle = -kPi * 0.5f;
    for (int i = 0; i < p->n; i++) {
        const PieSlice& s = p->slices[i];
        float sweep = 2.f * kPi * (s.value / total) - p->padAngle;
        if (sweep <= 0) {
            angle += 2.f * kPi * (s.value / total);
            continue;
        }
        float ro = p->outerRadius - s.outerInset;
        float ri = p->innerRadius;
        ID2D1PathGeometry* geo = nullptr;
        if (FAILED(ctx->d2d->CreatePathGeometry(&geo)) || !geo) {
            return;
        }
        ID2D1GeometrySink* sink = nullptr;
        if (SUCCEEDED(geo->Open(&sink)) && sink) {
            float a0 = angle, a1 = angle + sweep;
            D2D1_ARC_SIZE big =
                sweep > kPi ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL;
            sink->BeginFigure(
                D2D1::Point2F(cx + ro * cosf(a0), cy + ro * sinf(a0)),
                D2D1_FIGURE_BEGIN_FILLED);
            D2D1_ARC_SEGMENT arc = {};
            arc.point = D2D1::Point2F(cx + ro * cosf(a1), cy + ro * sinf(a1));
            arc.size = D2D1::SizeF(ro, ro);
            arc.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
            arc.arcSize = big;
            sink->AddArc(arc);
            if (ri > 0) {
                sink->AddLine(
                    D2D1::Point2F(cx + ri * cosf(a1), cy + ri * sinf(a1)));
                D2D1_ARC_SEGMENT inner = {};
                inner.point =
                    D2D1::Point2F(cx + ri * cosf(a0), cy + ri * sinf(a0));
                inner.size = D2D1::SizeF(ri, ri);
                inner.sweepDirection = D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;
                inner.arcSize = big;
                sink->AddArc(inner);
            } else {
                sink->AddLine(D2D1::Point2F(cx, cy));
            }
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            sink->Close();
            sink->Release();
        }
        ctx->brush->SetColor(RgbaToD2D(s.color));
        ctx->rt->FillGeometry(geo, ctx->brush, nullptr);
        geo->Release();
        angle += 2.f * kPi * (s.value / total);
    }
}

El* PieChart::IntoEl() {
    float d = outerRadius * 2;
    El* e = Div(a)->W(d)->H(d);
    e->customPaint = PaintPie;
    e->customUser = this;
    return e;
}

} // namespace component
} // namespace gpui
