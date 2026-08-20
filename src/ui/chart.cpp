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
    fillBottom = RgbaOpacity(c, 0.f);
    return this;
}

AreaChart* AreaChart::Fill(Rgba top, Rgba bottom) {
    fill = top;
    fillBottom = bottom;
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
AreaChart* AreaChart::Linear() {
    strokeStyle = ChartStroke::Linear;
    return this;
}
AreaChart* AreaChart::StepAfter() {
    strokeStyle = ChartStroke::StepAfter;
    return this;
}
El* AreaChart::IntoEl() {
    El* e = ChartEl(a, ys, n, stroke, fill, fillBottom, tickMargin);
    e->chart.labels = labels;
    e->chart.strokeStyle = strokeStyle;
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
LineChart* LineChart::Linear() {
    strokeStyle = ChartStroke::Linear;
    return this;
}
LineChart* LineChart::StepAfter() {
    strokeStyle = ChartStroke::StepAfter;
    return this;
}
LineChart* LineChart::Dot(bool v) {
    dot = v;
    return this;
}
El* LineChart::IntoEl() {
    Rgba none = {0, 0, 0, 0};
    El* e = ChartEl(a, ys, n, stroke, none, none, tickMargin);
    e->chart.kind = ChartKind::Line;
    e->chart.labels = labels;
    e->chart.strokeStyle = strokeStyle;
    e->chart.dot = dot;
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
BarChart* BarChart::Alignment(BarAlign v) {
    align = v;
    return this;
}
BarChart* BarChart::Base(const float* y0) {
    bases = y0;
    return this;
}
BarChart* BarChart::Overlay(bool v) {
    overlay = v;
    return this;
}
BarChart* BarChart::LabelValues(bool v) {
    labelValues = v;
    return this;
}
BarChart* BarChart::Fills(const Rgba* colors) {
    fills = colors;
    return this;
}
BarChart* BarChart::FillGradient(Rgba from, Rgba to, bool perBar) {
    gradient = true;
    gradientFrom = from;
    gradientTo = to;
    gradientPerBar = perBar;
    return this;
}
El* BarChart::IntoEl() {
    Rgba none = {0, 0, 0, 0};
    El* e = ChartEl(a, ys, n, fill, none, none, tickMargin);
    e->chart.kind = ChartKind::Bar;
    e->chart.labels = labels;
    e->chart.barAlign = align;
    e->chart.bases = bases;
    e->chart.overlay = overlay;
    e->chart.barLabels = labelValues;
    e->chart.barFills = fills;
    e->chart.barGradient = gradient;
    e->chart.barGradientPerBar = gradientPerBar;
    e->chart.barFillFrom = gradientFrom;
    e->chart.barFillTo = gradientTo;
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
CandlestickChart* CandlestickChart::BodyWidthRatio(float v) {
    bodyWidthRatio = v;
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
    e->chart.bodyWidthRatio = bodyWidthRatio;
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
RadarChart* RadarChart::Overlay(bool v) {
    overlay = v;
    return this;
}
RadarChart* RadarChart::Dot(bool v) {
    dot = v;
    return this;
}
RadarChart* RadarChart::OuterRadius(float v) {
    outerRadius = v;
    return this;
}
RadarChart* RadarChart::GridLevels(int v) {
    gridLevels = v;
    return this;
}
El* RadarChart::IntoEl() {
    Rgba none = {0, 0, 0, 0};
    El* e = ChartEl(a, values, n, stroke, fill, none, 1);
    e->chart.kind = ChartKind::Radar;
    e->chart.labels = labels;
    e->chart.overlay = overlay;
    e->chart.dot = dot;
    e->chart.radarRadius = outerRadius;
    e->chart.gridLevels = gridLevels;
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

// ─── SankeyChart ─────────────────────────────────────────────────────────

void SankeyChartThroughput(const SankeyLink* links, int nLinks, double* out,
                           int n) {
    // The two sides are added up separately and the larger wins, the same way
    // the layout works out a node's value — but in the caller's own units.
    for (int i = 0; i < n; i++) {
        double incoming = 0;
        double outgoing = 0;
        for (int k = 0; k < nLinks; k++) {
            if (links[k].target == i) {
                incoming += links[k].value;
            }
            if (links[k].source == i) {
                outgoing += links[k].value;
            }
        }
        out[i] = incoming > outgoing ? incoming : outgoing;
    }
}

// One line of a node's label. `align` is -1 for a label ending at x, 0 for
// one centred on it, 1 for one starting there.
static void SankeyLabelLine(PaintCtx* ctx, Str text, float x, float y,
                            float maxW, float fontSize, Rgba color, int align) {
    if (!text.s || text.len <= 0 || maxW <= 0) {
        return;
    }
    Size size = {};
    TextLayout* tl =
        TextLayoutNew(ctx, text, fontSize, maxW, false, 0, 0, &size);
    if (!tl) {
        return;
    }
    float left = x;
    if (align < 0) {
        left = x - size.w;
    } else if (align == 0) {
        left = x - size.w / 2.f;
    }
    TextLayoutDraw(ctx, tl, left, y, color, true);
    TextLayoutRelease(tl);
}

static void PaintSankey(PaintCtx* ctx, El* e, void* user) {
    auto* c = (SankeyChart*)user;
    if (!c || !ctx->rt || c->n == 0 || c->nLinks == 0) {
        return;
    }
    float width = e->w;
    float height = e->h;
    if (width <= 0 || height <= 0) {
        return;
    }

    Sankey gen;
    gen.nodeWidth = c->nodeWidth;
    gen.nodePadding = c->nodePadding;
    gen.align = c->align;
    gen.iterations = c->iterations;
    gen.valueScale = c->valueScale;

    // The first pass is the topology alone: the columns are all the label
    // margins need, and the margins are what the extent depends on.
    SankeyGraph g;
    if (SankeyTopology(&gen, c->n, c->links, c->nLinks, &g) !=
        SankeyError::None) {
        return;
    }
    int layers = SankeyLayerCount(&g);

    // The labels read the raw throughput; the layout's own value is in
    // scaled units under a non-linear scale.
    double raw[kMaxSankeyChartNodes] = {};
    SankeyChartThroughput(c->links, c->nLinks, raw, c->n);
    Str values[kMaxSankeyChartNodes] = {};
    for (int i = 0; i < c->n; i++) {
        values[i] = c->showValues ? fmt("%.0f", raw[i]) : c->nodes[i].value;
    }

    bool hasLabels = false;
    for (int i = 0; i < c->n; i++) {
        if (c->nodes[i].label.s || values[i].s) {
            hasLabels = true;
        }
    }

    // The margin the labels beside the first and last columns need, each side
    // capped so one long label cannot take the flow area over.
    float left = 0;
    float right = 0;
    if (hasLabels) {
        for (int i = 0; i < c->n; i++) {
            int layer = g.nodes[i].layer;
            if (layer != 0 && layer + 1 != layers) {
                continue;
            }
            float labelW = 0;
            Str lines[3] = {values[i], c->nodes[i].note, c->nodes[i].label};
            for (int k = 0; k < 3; k++) {
                if (!lines[k].s) {
                    continue;
                }
                Size sz = MeasureText(ctx, lines[k], kPlotTextSize, 0);
                if (sz.w > labelW) {
                    labelW = sz.w;
                }
            }
            float want = labelW + c->labelGap;
            if (layer == 0) {
                left = want > left ? want : left;
            } else {
                right = want > right ? want : right;
            }
        }
        float cap = width * kSankeyMaxLabelWidthRatio;
        left = left < cap ? left : cap;
        right = right < cap ? right : cap;
    }
    // A middle column's labels sit above its nodes, so the top band is
    // reserved for the tallest of those blocks.
    float lineH = kPlotTextSize + kPlotTextGap;
    float top = 0;
    if (hasLabels && layers > 2) {
        for (int i = 0; i < c->n; i++) {
            int layer = g.nodes[i].layer;
            if (layer == 0 || layer + 1 == layers) {
                continue;
            }
            float block = 0;
            if (values[i].s) {
                block += lineH;
            }
            if (c->nodes[i].label.s) {
                block += lineH;
            }
            if (block > 0 && block + kPlotTextGap > top) {
                top = block + kPlotTextGap;
            }
        }
    }
    float bottom = hasLabels ? kPlotTextGap : 0.f;
    float maxVertical = height * kSankeyMaxLabelMarginRatio;
    if (top + bottom > maxVertical) {
        float k = maxVertical / (top + bottom);
        top *= k;
        bottom *= k;
    }

    // The second pass places the nodes on what the labels left over.
    gen.x0 = left;
    gen.y0 = top;
    gen.x1 = width - right > left + 1 ? width - right : left + 1;
    gen.y1 = height - bottom > top + 1 ? height - bottom : top + 1;
    SankeyLayoutFrom(&gen, &g);

    // chart_1..chart_5 in Rust: the palette a node falls back to, by index.
    const Theme& th = ThemeNow();
    Rgba palette[5] = {th.blue, th.green, th.yellow, th.magenta, th.cyan};
    Rgba colors[kMaxSankeyChartNodes] = {};
    for (int i = 0; i < c->n; i++) {
        colors[i] = c->nodes[i].hasColor ? c->nodes[i].color : palette[i % 5];
    }

    // The ribbons first, under the nodes: a horizontal cubic through the
    // midpoint, thickened to each end's own width, and filled from the colour
    // it leaves to the colour it arrives at.
    for (int i = 0; i < g.links.len; i++) {
        const SankeyLinkLayout& link = g.links[i];
        if (link.value <= 0) {
            continue;
        }
        const SankeyNodeLayout& source = g.nodes[link.source];
        const SankeyNodeLayout& target = g.nodes[link.target];
        float sw = link.sourceWidth > c->minLinkWidth ? link.sourceWidth
                                                      : c->minLinkWidth;
        float tw = link.targetWidth > c->minLinkWidth ? link.targetWidth
                                                      : c->minLinkWidth;
        float sourceHalf = sw / 2.f;
        float targetHalf = tw / 2.f;
        float sx = e->x + source.x1;
        float tx = e->x + target.x0;
        float mx = (sx + tx) / 2.f;
        float sy = e->y + link.y0;
        float ty = e->y + link.y1;
        Path* p = PathNew(ctx, true);
        if (!p) {
            continue;
        }
        PathMoveTo(p, sx, sy - sourceHalf);
        PathCubicTo(p, mx, sy - sourceHalf, mx, ty - targetHalf, tx,
                    ty - targetHalf);
        PathLineTo(p, tx, ty + targetHalf);
        PathCubicTo(p, mx, ty + targetHalf, mx, sy + sourceHalf, sx,
                    sy + sourceHalf);
        PathClose(p);
        PathFillGradient(ctx, p, sx, sy, tx, ty,
                         RgbaOpacity(colors[link.source], c->linkOpacity),
                         RgbaOpacity(colors[link.target], c->linkOpacity));
        PathFree(p);
    }

    for (int i = 0; i < g.nodes.len; i++) {
        const SankeyNodeLayout& node = g.nodes[i];
        // A node carrying almost nothing still gets a pixel to be seen by.
        float y1 = node.y1 > node.y0 + 1 ? node.y1 : node.y0 + 1;
        FillRound(ctx, e->x + node.x0, e->y + node.y0, node.x1 - node.x0,
                  y1 - node.y0, c->nodeRadius, colors[node.index]);
    }

    if (!hasLabels) {
        return;
    }
    for (int i = 0; i < g.nodes.len; i++) {
        const SankeyNodeLayout& node = g.nodes[i];
        Str lines[3] = {values[node.index], c->nodes[node.index].note,
                        c->nodes[node.index].label};
        Rgba lineColors[3] = {th.foreground, c->nodes[node.index].noteColor,
                              th.mutedFg};
        float block = 0;
        for (int k = 0; k < 3; k++) {
            if (lines[k].s) {
                block += lineH;
            }
        }
        if (block <= 0) {
            continue;
        }

        bool isFirst = node.layer == 0;
        bool isLast = node.layer + 1 == layers;
        // Beside the first and last columns, above the middle ones — and
        // bounded, so a long label is clipped rather than drawn off the plot.
        float x = 0;
        float maxW = 0;
        int align = 0;
        if (isFirst) {
            x = node.x0 - c->labelGap;
            align = -1;
            maxW = left - c->labelGap;
        } else if (isLast) {
            x = node.x1 + c->labelGap;
            align = 1;
            maxW = right - c->labelGap;
        } else {
            float center = (node.x0 + node.x1) / 2.f;
            x = center;
            align = 0;
            float toEdge = center < width - center ? center : width - center;
            maxW = 2.f * toEdge;
        }

        float y = 0;
        if (isFirst || isLast) {
            // Centred beside the node, and kept inside the plot so a node at
            // either edge does not lose its label.
            y = (node.y0 + node.y1) / 2.f - block / 2.f;
            if (y > height - block) {
                y = height - block;
            }
            if (y < 0) {
                y = 0;
            }
        } else {
            y = node.y0 - block - kPlotTextGap;
        }
        for (int k = 0; k < 3; k++) {
            if (!lines[k].s) {
                continue;
            }
            SankeyLabelLine(ctx, lines[k], e->x + x, e->y + y, maxW,
                            kPlotTextSize, lineColors[k], align);
            y += lineH;
        }
    }
}

SankeyChart* SankeyChart::New(Ctx* cx) {
    Arena* a = cx->a;
    SankeyChart* c = ArenaNew<SankeyChart>(a);
    c->a = a;
    c->cx = cx;
    return c;
}
SankeyChart* SankeyChart::Node(Str label) {
    if (n < kMaxSankeyChartNodes) {
        nodes[n].label = label;
        n++;
    }
    return this;
}
SankeyChart* SankeyChart::NodeValue(Str text) {
    if (n > 0) {
        nodes[n - 1].value = text;
    }
    return this;
}
SankeyChart* SankeyChart::NodeNote(Str text, Rgba color) {
    if (n > 0) {
        nodes[n - 1].note = text;
        nodes[n - 1].noteColor = color;
    }
    return this;
}
SankeyChart* SankeyChart::NodeColored(Str label, Rgba color) {
    if (n < kMaxSankeyChartNodes) {
        nodes[n].label = label;
        nodes[n].color = color;
        nodes[n].hasColor = true;
        n++;
    }
    return this;
}
SankeyChart* SankeyChart::Link(int source, int target, double value) {
    if (nLinks < kMaxSankeyChartLinks) {
        links[nLinks].source = source;
        links[nLinks].target = target;
        links[nLinks].value = value;
        nLinks++;
    }
    return this;
}
SankeyChart* SankeyChart::NodeWidth(float v) {
    nodeWidth = v;
    return this;
}
SankeyChart* SankeyChart::NodePadding(float v) {
    nodePadding = v;
    return this;
}
SankeyChart* SankeyChart::NodeAlign(SankeyAlign v) {
    align = v;
    return this;
}
SankeyChart* SankeyChart::Iterations(int v) {
    iterations = v;
    return this;
}
SankeyChart* SankeyChart::ValueScale(SankeyValueScale v) {
    valueScale = v;
    return this;
}
SankeyChart* SankeyChart::NodeCornerRadius(float v) {
    nodeRadius = v;
    return this;
}
SankeyChart* SankeyChart::LinkOpacity(float v) {
    linkOpacity = v;
    return this;
}
SankeyChart* SankeyChart::MinLinkWidth(float v) {
    minLinkWidth = v;
    return this;
}
SankeyChart* SankeyChart::LabelGap(float v) {
    labelGap = v;
    return this;
}
SankeyChart* SankeyChart::ShowValues(bool v) {
    showValues = v;
    return this;
}
El* SankeyChart::IntoEl() {
    El* e = Div(a);
    e->customPaint = PaintSankey;
    e->customUser = this;
    return e;
}

} // namespace component
} // namespace gpui
