#include "ui/chart.h"
#include "gpui/paint.h"

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

AreaChart* AreaChart::Tooltip(Str name) {
    tooltipName = name;
    tooltip = true;
    return this;
}
El* AreaChart::IntoEl() {
    El* e =
        ChartEl(a, ys, n, stroke, fill, RgbaOpacity(fill, 0.0f), tickMargin);
    e->chart.labels = labels;
    e->chart.overlay = overlay;
    e->chart.tooltip = tooltip;
    e->chart.name = tooltipName;
    return e;
}

LineChart* LineChart::New(Ctx* cx, const float* ys, int n) {
    Arena* a = cx->a;
    LineChart* c = ArenaNew<LineChart>(a);
    c->a = a;
    c->cx = cx;
    c->ys = ys;
    c->n = n;
    c->stroke = cx->theme().blue;
    return c;
}
LineChart* LineChart::Stroke(Rgba c) {
    stroke = c;
    return this;
}
LineChart* LineChart::Labels(const char* const* l) {
    labels = l;
    return this;
}
LineChart* LineChart::TickMargin(int t) {
    tickMargin = t;
    return this;
}
LineChart* LineChart::Domain(float lo, float hi) {
    domainMin = lo;
    domainMax = hi;
    return this;
}
LineChart* LineChart::Tooltip(Str name) {
    tooltipName = name;
    tooltip = true;
    return this;
}
El* LineChart::IntoEl() {
    Rgba none = {0, 0, 0, 0};
    El* e = ChartEl(a, ys, n, stroke, none, none, tickMargin);
    e->chart.kind = ChartKind::Line;
    e->chart.labels = labels;
    e->chart.domainMin = domainMin;
    e->chart.domainMax = domainMax;
    e->chart.tooltip = tooltip;
    e->chart.name = tooltipName;
    return e;
}

BarChart* BarChart::New(Ctx* cx, const float* ys, int n) {
    Arena* a = cx->a;
    BarChart* c = ArenaNew<BarChart>(a);
    c->a = a;
    c->cx = cx;
    c->ys = ys;
    c->n = n;
    c->fill = cx->theme().primary;
    return c;
}
BarChart* BarChart::Fill(Rgba c) {
    fill = c;
    return this;
}
BarChart* BarChart::Labels(const char* const* l) {
    labels = l;
    return this;
}
BarChart* BarChart::TickMargin(int t) {
    tickMargin = t;
    return this;
}
BarChart* BarChart::Padding(float v) {
    padding = v;
    return this;
}
BarChart* BarChart::Radius(float v) {
    radius = v;
    return this;
}
BarChart* BarChart::Domain(float lo, float hi) {
    domainMin = lo;
    domainMax = hi;
    return this;
}
BarChart* BarChart::Tooltip(Str name) {
    tooltipName = name;
    tooltip = true;
    return this;
}
El* BarChart::IntoEl() {
    Rgba none = {0, 0, 0, 0};
    El* e = ChartEl(a, ys, n, fill, none, none, tickMargin);
    e->chart.kind = ChartKind::Bar;
    e->chart.labels = labels;
    e->chart.bandPadding = padding;
    e->chart.barRadius = radius;
    e->chart.domainMin = domainMin;
    e->chart.domainMax = domainMax;
    e->chart.tooltip = tooltip;
    e->chart.name = tooltipName;
    return e;
}

CandlestickChart* CandlestickChart::New(Ctx* cx, const float* opens,
                                        const float* highs, const float* lows,
                                        const float* closes, int n) {
    Arena* a = cx->a;
    CandlestickChart* c = ArenaNew<CandlestickChart>(a);
    c->a = a;
    c->cx = cx;
    c->opens = opens;
    c->highs = highs;
    c->lows = lows;
    c->closes = closes;
    c->n = n;
    c->up = cx->theme().green;
    c->down = cx->theme().red;
    return c;
}
CandlestickChart* CandlestickChart::Colors(Rgba u, Rgba d) {
    up = u;
    down = d;
    return this;
}
CandlestickChart* CandlestickChart::Labels(const char* const* l) {
    labels = l;
    return this;
}
CandlestickChart* CandlestickChart::TickMargin(int t) {
    tickMargin = t;
    return this;
}
CandlestickChart* CandlestickChart::Padding(float v) {
    padding = v;
    return this;
}
El* CandlestickChart::IntoEl() {
    Rgba none = {0, 0, 0, 0};
    // The closes are the series; the other three ride along beside them.
    El* e = ChartEl(a, closes, n, up, none, none, tickMargin);
    e->chart.kind = ChartKind::Candlestick;
    e->chart.labels = labels;
    e->chart.opens = opens;
    e->chart.highs = highs;
    e->chart.lows = lows;
    e->chart.up = up;
    e->chart.down = down;
    e->chart.bandPadding = padding;
    return e;
}

RadarChart* RadarChart::New(Ctx* cx, const float* values, int n) {
    Arena* a = cx->a;
    RadarChart* c = ArenaNew<RadarChart>(a);
    c->a = a;
    c->cx = cx;
    c->values = values;
    c->n = n;
    c->stroke = cx->theme().blue;
    c->fill = RgbaOpacity(cx->theme().blue, 0.3f);
    return c;
}
RadarChart* RadarChart::Stroke(Rgba c) {
    stroke = c;
    return this;
}
RadarChart* RadarChart::Fill(Rgba c) {
    fill = c;
    return this;
}
RadarChart* RadarChart::Labels(const char* const* l) {
    labels = l;
    return this;
}
RadarChart* RadarChart::Domain(float lo, float hi) {
    domainMin = lo;
    domainMax = hi;
    return this;
}
El* RadarChart::IntoEl() {
    Rgba none = {0, 0, 0, 0};
    El* e = ChartEl(a, values, n, stroke, fill, none, 1);
    e->chart.kind = ChartKind::Radar;
    e->chart.labels = labels;
    e->chart.domainMin = domainMin;
    e->chart.domainMax = domainMax;
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
    if (!p || !ctx->rt || p->n == 0) {
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
        float a0 = angle, a1 = angle + sweep;
        Path* wedge = PathNew(ctx, true);
        if (wedge) {
            PathArcTo(wedge, cx, cy, ro, a0, a1, true);
            if (ri > 0) {
                // Back along the inner radius to close the donut segment.
                PathArcTo(wedge, cx, cy, ri, a1, a0, false);
            } else {
                PathLineTo(wedge, cx, cy);
            }
            PathClose(wedge);
            PathFill(ctx, wedge, s.color);
            PathFree(wedge);
        }
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
