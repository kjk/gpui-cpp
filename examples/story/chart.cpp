#include "Story.h"
#include "ChartFixtures.h"

// cosf and sinf, for the radar chart's badge labels: MSVC hands them over
// with the rest of the runtime, gcc does not.
#include <math.h>

struct ChartStory {
    static El* Render(ChartStory* self, Ctx* cx);
};

// chart_container(): a 400px card with the title, the range, the chart and
// two lines of commentary.
static El* ChartCard(Ctx* cx, const char* title, El* chart, bool center) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow(cx->app);
    El* card = Div(a)
                   ->FlexCol()
                   // flex_1. In Rust a wrapping row breaks its lines on each
                   // card's min-content width, so two of these land per line;
                   // layout here shares the row out first and wraps on what
                   // is left, so more fit and the chart inside overflows.
                   ->Flex1()
                   ->H(400)
                   ->Pad(16)
                   ->Radius(th.radiusLg)
                   ->Border(1, th.border);
    El* head = StoryTxt(cx, Str(title), 16, th.foreground)->Semibold();
    El* sub = StoryTxt(cx, StrL("January-June 2025"), 14, th.mutedFg);
    El* foot1 =
        StoryTxt(cx, StrL("Trending up by 5.2% this month"), 14, th.foreground)
            ->Semibold();
    El* foot2 = StoryTxt(cx,
                         StrL("Showing total visitors for the last 6 "
                              "months"),
                         14, th.mutedFg);
    if (center) {
        card->Child(Div(a)->W(kFill)->FlexRow()->JustifyCenter()->Child(head));
        card->Child(Div(a)->W(kFill)->FlexRow()->JustifyCenter()->Child(sub));
    } else {
        card->Child(head);
        card->Child(sub);
    }
    El* body = Div(a)->Flex1()->W(kFill)->PadY(16)->FlexRow();
    if (center) {
        body->ItemsCenter()->JustifyCenter();
    }
    body->Child(chart);
    card->Child(body);
    if (center) {
        card->Child(Div(a)->W(kFill)->FlexRow()->JustifyCenter()->Child(foot1));
        card->Child(Div(a)->W(kFill)->FlexRow()->JustifyCenter()->Child(foot2));
    } else {
        card->Child(foot1);
        card->Child(foot2);
    }
    return card;
}

// h_flex().flex_wrap().gap_4(): the row each group of cards sits in.
static El* ChartRow(Ctx* cx) {
    return Div(cx->a)->FlexRow()->W(kFill)->Gap(16)->FlexWrap();
}

El* ChartStory::Render(ChartStory*, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow(cx->app);
    // `let color = cx.theme().chart_3`, which every pie and mixed bar tints
    // by its own alpha.
    Rgba color = th.chart3;
    El* page = Div(a)->FlexCol()->Gap(16)->W(kFill);

    // Area Chart - Stacked: one chart with two series, desktop and mobile,
    // both from daily-devices.json — `.y(..).stroke(..).fill(..).name(..)`
    // twice over one set of axes, which is what the Rust story writes.
    El* areaBox =
        component::AreaChart::New(cx, kDailyDesktop, kDailyDeviceCount)
            ->Tooltip(StrL("Desktop"))
            ->Stroke(th.chart1)
            ->Fill(RgbaOpacity(th.chart1, 0.4f),
                   RgbaOpacity(th.background, 0.3f))
            ->Y(kDailyMobile)
            ->Stroke(th.chart2)
            ->Fill(RgbaOpacity(th.chart2, 0.4f),
                   RgbaOpacity(th.background, 0.3f))
            ->Tooltip(StrL("Mobile"))
            ->Labels(kDailyDate)
            ->TickMargin(8)
            ->IntoEl()
            ->W(kFill)
            ->H(kFill);
    page->Child(ChartCard(cx, "Area Chart - Stacked", areaBox, false));

    // The four pies, all off monthly-devices.json.
    El* pieRow = ChartRow(cx);
    component::PieChart* pie = component::PieChart::New(cx)->OuterRadius(100);
    component::PieChart* donut =
        component::PieChart::New(cx)->OuterRadius(100)->InnerRadius(60);
    component::PieChart* padded = component::PieChart::New(cx)
                                      ->OuterRadius(100)
                                      ->InnerRadius(60)
                                      ->PadAngle(4.f / 100.f);
    component::PieChart* labelled =
        component::PieChart::New(cx)->OuterRadius(80)->InnerRadius(50);
    for (int i = 0; i < kMonthlyDeviceCount; i++) {
        Rgba c = RgbaOpacity(color, kMonthlyAlpha[i]);
        pie->Slice(kMonthlyDesktop[i], c);
        // outer_radius_fn(|d| 100. - d.index * 4.).
        donut->Slice(kMonthlyDesktop[i], c, (float)i * 4.f);
        padded->Slice(kMonthlyDesktop[i], c);
        labelled->Slice(kMonthlyDesktop[i], c)->Label(Str(kMonthlyMonth[i]));
    }
    pieRow->Child(ChartCard(cx, "Pie Chart", pie->IntoEl(), true));
    pieRow->Child(ChartCard(cx, "Pie Chart - Donut", donut->IntoEl(), true));
    pieRow
        ->Child(ChartCard(cx, "Pie Chart - Pad Angle", padded->IntoEl(), true));
    pieRow->Child(ChartCard(cx, "Pie Chart - Label", labelled->IntoEl(), true));
    page->Child(pieRow);
    page->Child(component::Separator::Horizontal(cx)->IntoEl());

    // The radars, off radar-devices.json.
    El* radarRow = ChartRow(cx);
    radarRow->Child(ChartCard(
        cx, "Radar Chart",
        component::RadarChart::New(cx, kRadarDesktop, kRadarDeviceCount)
            ->Labels(kRadarMonth)
            ->IntoEl()
            ->W(kFill)
            ->H(kFill),
        true));
    // Radar Chart - Multiple: a second ring over the first one's grid.
    El* radarMulti = Div(a)->W(kFill)->H(kFill);
    radarMulti
        ->Child(component::RadarChart::New(cx, kRadarDesktop, kRadarDeviceCount)
                    ->Labels(kRadarMonth)
                    ->IntoEl()
                    ->W(kFill)
                    ->H(kFill));
    radarMulti
        ->Child(component::RadarChart::New(cx, kRadarMobile, kRadarDeviceCount)
                    ->Stroke(th.chart2)
                    ->Fill(RgbaOpacity(th.chart2, 0.3f))
                    ->Overlay()
                    ->IntoEl()
                    ->Absolute()
                    ->Left(0)
                    ->Top(0)
                    ->W(kFill)
                    ->H(kFill));
    radarRow->Child(ChartCard(cx, "Radar Chart - Multiple", radarMulti, true));
    // Radar Chart - Dots: an element label — the month over a grade badge —
    // so the ring pulls in to outer_radius(64.) to leave it room.
    component::RadarLabel* radarLabels = (component::RadarLabel*)Alloc(
        a, sizeof(component::RadarLabel) * kRadarDeviceCount);
    for (int i = 0; i < kRadarDeviceCount; i++) {
        const char* grade = kRadarDesktop[i] >= 250.f   ? "A"
                            : kRadarDesktop[i] >= 200.f ? "B"
                                                        : "C";
        El* badge = Div(a)->FlexCol()->ItemsCenter()->Gap(4);
        badge->Child(StoryTxt(cx, Str(kRadarMonth[i]), 12, th.mutedFg));
        badge->Child(Div(a)
                         ->FlexRow()
                         ->W(24)
                         ->H(24)
                         ->ItemsCenter()
                         ->JustifyCenter()
                         ->Radius(99)
                         ->Bg(RgbaOpacity(th.chart2, 0.1f))
                         ->Child(StoryTxt(cx, Str(grade), 14, th.chart2)
                                     ->Semibold()
                                     ->LineHeight(1.f)));
        radarLabels[i] = component::RadarLabel::Element(badge);
    }
    El* radarDots =
        component::RadarChart::New(cx, kRadarDesktop, kRadarDeviceCount)
            ->Labels(radarLabels)
            ->Stroke(th.chart2)
            ->Fill(RgbaOpacity(th.chart2, 0.3f))
            ->Dot()
            ->OuterRadius(64)
            ->IntoEl()
            ->W(kFill)
            ->H(kFill);
    radarRow->Child(ChartCard(cx, "Radar Chart - Dots", radarDots, true));
    // Radar Chart - Lines Only: max_value(400) and no fill under the ring.
    radarRow->Child(ChartCard(
        cx, "Radar Chart - Lines Only",
        component::RadarChart::New(cx, kRadarDesktop, kRadarDeviceCount)
            ->Labels(kRadarMonth)
            ->Stroke(th.chart3)
            ->Fill(Rgba8(0, 0, 0, 0))
            ->Domain(0, 400)
            ->GridLevels(5)
            ->IntoEl()
            ->W(kFill)
            ->H(kFill),
        true));
    page->Child(radarRow);
    page->Child(component::Separator::Horizontal(cx)->IntoEl());

    // The bars, off monthly-devices.json.
    El* barRow = ChartRow(cx);
    barRow->Child(ChartCard(
        cx, "Bar Chart",
        component::BarChart::New(cx, kMonthlyDesktop, kMonthlyDeviceCount)
            ->Fill(th.chart1)
            ->Labels(kMonthlyMonth)
            ->Tooltip(StrL("Desktop"))
            ->TickMargin(1)
            ->IntoEl()
            ->W(kFill)
            ->H(kFill),
        false));
    // Bar Chart - Negative values: the monthly figures recentred on their
    // mean, so the bars have a mix of signs to draw around the zero line, and
    // the value axis switched on beside them.
    static float variations[kMonthlyDeviceCount];
    static bool variationsReady = false;
    if (!variationsReady) {
        variationsReady = true;
        float sum = 0;
        for (int i = 0; i < kMonthlyDeviceCount; i++) {
            sum += kMonthlyDesktop[i];
        }
        float mean = sum / (float)kMonthlyDeviceCount;
        for (int i = 0; i < kMonthlyDeviceCount; i++) {
            float v = kMonthlyDesktop[i] - mean;
            variations[i] = (float)lroundf(v);
        }
    }
    barRow->Child(
        ChartCard(cx, "Bar Chart - Negative values",
                  component::BarChart::New(cx, variations, kMonthlyDeviceCount)
                      ->Fill(th.chart1)
                      ->Labels(kMonthlyMonth)
                      ->Tooltip(StrL("Variation"))
                      ->TickMargin(1)
                      ->LabelValues()
                      ->ValueAxis()
                      ->IntoEl()
                      ->W(kFill)
                      ->H(kFill),
                  false));

    // Bar Chart - Mixed: fill(|d, ..| d.color(color)), a tint per bar.
    static Rgba mixed[kMonthlyDeviceCount];
    for (int i = 0; i < kMonthlyDeviceCount; i++) {
        mixed[i] = RgbaOpacity(color, kMonthlyAlpha[i]);
    }
    barRow->Child(ChartCard(
        cx, "Bar Chart - Mixed",
        component::BarChart::New(cx, kMonthlyDesktop, kMonthlyDeviceCount)
            ->Fills(mixed)
            ->Labels(kMonthlyMonth)
            ->TickMargin(1)
            ->IntoEl()
            ->W(kFill)
            ->H(kFill),
        false));

    // Bar Chart - Stacked: Stack::keys(desktop, mobile, tablet, watch) over
    // the first eight days, drawn as four series each sitting on the running
    // total of the ones below it.
    const int kStackDays = 8;
    const float* kStackSeries[4] = {kDailyDesktop, kDailyMobile, kDailyTablet,
                                    kDailyWatch};
    Rgba kStackColors[4] = {th.chart4, th.chart3, th.chart2, th.chart1};
    El* stacked = Div(a)->W(kFill)->H(kFill);
    auto* bases = (float*)Alloc(a, (int)sizeof(float) * kStackDays * 5);
    for (int d = 0; d < kStackDays; d++) {
        bases[d] = 0;
    }
    for (int k = 0; k < 4; k++) {
        float* base = bases + k * kStackDays;
        float* next = bases + (k + 1) * kStackDays;
        auto* tops = (float*)Alloc(a, (int)sizeof(float) * kStackDays);
        for (int d = 0; d < kStackDays; d++) {
            tops[d] = base[d] + kStackSeries[k][d];
            next[d] = tops[d];
        }
        component::BarChart* bar =
            component::BarChart::New(cx, tops, kStackDays)
                ->Fill(kStackColors[k])
                ->Base(base)
                ->Padding(0.4f)
                ->Radius(0)
                ->TickMargin(1)
                ->Labels(kDailyDate);
        // Every series is scaled against the full stack, so they line up.
        bar->Domain(0, bases[4 * kStackDays]);
        float top = 0;
        for (int d = 0; d < kStackDays; d++) {
            if (bases[4 * kStackDays + d] > top) {
                top = bases[4 * kStackDays + d];
            }
        }
        bar->Domain(0, top);
        if (k > 0) {
            bar->Overlay();
        }
        El* el = bar->IntoEl()->W(kFill)->H(kFill);
        if (k > 0) {
            el->Absolute()->Left(0)->Top(0);
        }
        stacked->Child(el);
    }
    barRow->Child(ChartCard(cx, "Bar Chart - Stacked", stacked, false));

    // Bar Chart - Rounded corners: corner_radii(px(8.)).
    barRow->Child(ChartCard(
        cx, "Bar Chart - Rounded corners",
        component::BarChart::New(cx, kMonthlyDesktop, kMonthlyDeviceCount)
            ->Fill(th.chart1)
            ->Labels(kMonthlyMonth)
            ->TickMargin(1)
            ->Radius(8)
            ->LabelValues()
            ->IntoEl()
            ->W(kFill)
            ->H(kFill),
        false));

    // The four alignments, all with the value written at the growing end.
    struct AlignCard {
        const char* title;
        BarAlign align;
    };
    static const AlignCard kAligns[] = {
        {"Bar Chart - Bottom aligned", BarAlign::Bottom},
        {"Bar Chart - Top aligned", BarAlign::Top},
        {"Bar Chart - Left aligned", BarAlign::Left},
        {"Bar Chart - Right aligned", BarAlign::Right},
    };
    for (const AlignCard& ac : kAligns) {
        barRow->Child(ChartCard(
            cx, ac.title,
            component::BarChart::New(cx, kMonthlyDesktop, kMonthlyDeviceCount)
                ->Fill(th.chart1)
                ->Labels(kMonthlyMonth)
                ->TickMargin(1)
                ->Alignment(ac.align)
                ->LabelValues()
                ->IntoEl()
                ->W(kFill)
                ->H(kFill),
            false));
    }

    // fill_gradient: four alignments of the chart-wide ramp, then the per-bar
    // one. The sixth is fill(|_, bar, chart, _|) instead — one ramp across
    // the whole plot's diagonal, each bar showing its own slice of it — so it
    // is built below rather than in this table.
    struct GradCard {
        const char* title;
        BarAlign align;
        bool perBar;
    };
    static const GradCard kGrads[] = {
        {"Bar Chart - Gradient (Bottom)", BarAlign::Bottom, false},
        {"Bar Chart - Gradient (Top)", BarAlign::Top, false},
        {"Bar Chart - Gradient (Left)", BarAlign::Left, false},
        {"Bar Chart - Gradient (Right)", BarAlign::Right, false},
        {"Bar Chart - Gradient (Per-bar)", BarAlign::Bottom, true},
    };
    for (const GradCard& gc : kGrads) {
        barRow->Child(ChartCard(
            cx, gc.title,
            component::BarChart::New(cx, kMonthlyDesktop, kMonthlyDeviceCount)
                ->Labels(kMonthlyMonth)
                ->TickMargin(1)
                ->Alignment(gc.align)
                ->LabelValues()
                ->FillGradient(RgbaOpacity(th.chart1, 0.3f), th.chart1,
                               gc.perBar)
                ->IntoEl()
                ->W(kFill)
                ->H(kFill),
            false));
    }
    barRow->Child(ChartCard(
        cx, "Bar Chart - Gradient (Diagonal, across bars)",
        component::BarChart::New(cx, kMonthlyDesktop, kMonthlyDeviceCount)
            ->Labels(kMonthlyMonth)
            ->TickMargin(1)
            ->LabelValues()
            ->FillGradientDiagonal(th.chart1, th.chart5)
            ->IntoEl()
            ->W(kFill)
            ->H(kFill),
        false));
    page->Child(barRow);
    page->Child(component::Separator::Horizontal(cx)->IntoEl());

    // The line chart, and the candlesticks off stock-prices.json.
    El* lineRow = ChartRow(cx);
    lineRow->Child(ChartCard(
        cx, "Line Chart - Tooltip",
        component::LineChart::New(cx, kMonthlyDesktop, kMonthlyDeviceCount)
            ->Stroke(th.chart1)
            ->Labels(kMonthlyMonth)
            ->Tooltip(StrL("Desktop"))
            ->TickMargin(1)
            ->IntoEl()
            ->W(kFill)
            ->H(kFill),
        false));
    lineRow->Child(ChartCard(
        cx, "Line Chart - Linear",
        component::LineChart::New(cx, kMonthlyDesktop, kMonthlyDeviceCount)
            ->Stroke(th.chart1)
            ->Labels(kMonthlyMonth)
            ->TickMargin(1)
            ->Linear()
            ->IntoEl()
            ->W(kFill)
            ->H(kFill),
        false));
    lineRow->Child(ChartCard(
        cx, "Line Chart - Step After",
        component::LineChart::New(cx, kMonthlyDesktop, kMonthlyDeviceCount)
            ->Stroke(th.chart1)
            ->Labels(kMonthlyMonth)
            ->TickMargin(1)
            ->StepAfter()
            ->IntoEl()
            ->W(kFill)
            ->H(kFill),
        false));
    lineRow->Child(ChartCard(
        cx, "Line Chart - Dots",
        component::LineChart::New(cx, kMonthlyDesktop, kMonthlyDeviceCount)
            ->Stroke(th.chart5)
            ->Labels(kMonthlyMonth)
            ->TickMargin(1)
            ->Dot()
            ->IntoEl()
            ->W(kFill)
            ->H(kFill),
        false));
    page->Child(lineRow);
    page->Child(component::Separator::Horizontal(cx)->IntoEl());

    // The four single-series area charts, which differ only in how the run of
    // points is joined and what is under it.
    El* areaRow = ChartRow(cx);
    struct AreaCard {
        const char* title;
        int stroke; // 0 natural, 1 linear, 2 step-after
        bool gradient;
    };
    static const AreaCard kAreas[] = {
        {"Area Chart", 0, false},
        {"Area Chart - Linear", 1, false},
        {"Area Chart - Step After", 2, false},
        {"Area Chart - Linear Gradient", 0, true},
    };
    for (const AreaCard& ac : kAreas) {
        component::AreaChart* ch =
            component::AreaChart::New(cx, kMonthlyDesktop, kMonthlyDeviceCount)
                ->Stroke(th.chart1)
                ->Labels(kMonthlyMonth)
                ->TickMargin(1);
        if (ac.stroke == 1) {
            ch->Linear();
        } else if (ac.stroke == 2) {
            ch->StepAfter();
        }
        if (ac.gradient) {
            ch->Fill(RgbaOpacity(th.chart1, 0.4f),
                     RgbaOpacity(th.background, 0.3f));
        } else {
            ch->Fill(RgbaOpacity(th.chart1, 0.2f));
        }
        areaRow->Child(
            ChartCard(cx, ac.title, ch->IntoEl()->W(kFill)->H(kFill), false));
    }
    page->Child(areaRow);
    page->Child(component::Separator::Horizontal(cx)->IntoEl());

    // The candlesticks, off stock-prices.json.
    El* candleRow = ChartRow(cx);
    candleRow->Child(ChartCard(
        cx, "Candlestick Chart",
        component::CandlestickChart::New(cx, kStockOpen, kStockHigh, kStockLow,
                                         kStockClose, kStockPriceCount)
            ->Colors(th.chartBullish, th.chartBearish)
            ->Labels(kStockDate)
            ->TickMargin(1)
            ->IntoEl()
            ->W(kFill)
            ->H(kFill),
        false));
    // body_width_ratio: half a band, then the whole of it.
    struct CandleCard {
        const char* title;
        float ratio;
        int tickMargin;
    };
    static const CandleCard kCandles[] = {
        {"Candlestick Chart - Narrow", 0.5f, 1},
        {"Candlestick Chart - Wide", 1.0f, 1},
        {"Candlestick Chart - Tick Margin", 0.8f, 2},
    };
    for (const CandleCard& cc : kCandles) {
        candleRow
            ->Child(ChartCard(cx, cc.title,
                              component::CandlestickChart::New(
                                  cx, kStockOpen, kStockHigh, kStockLow,
                                  kStockClose, kStockPriceCount)
                                  ->Colors(th.chartBullish, th.chartBearish)
                                  ->Labels(kStockDate)
                                  ->TickMargin(cc.tickMargin)
                                  ->BodyWidthRatio(cc.ratio)
                                  ->IntoEl()
                                  ->W(kFill)
                                  ->H(kFill),
                              false));
    }
    page->Child(candleRow);
    page->Child(component::Separator::Horizontal(cx)->IntoEl());

    // The two TSLA income statements, each a sankey of its own. A sqrt
    // value scale keeps the revenue flow from dwarfing the small profit and
    // expense ones, and the nodes carry the fixture's own colours.
    El* sankeyRow = ChartRow(cx);
    const TslaNode* kTslaNodes[kTslaStatementCount] = {kTsla0Nodes,
                                                       kTsla1Nodes};
    const TslaLink* kTslaLinks[kTslaStatementCount] = {kTsla0Links,
                                                       kTsla1Links};
    for (int st = 0; st < kTslaStatementCount; st++) {
        component::SankeyChart* sk = component::SankeyChart::New(cx)
                                         ->NodeAlign(SankeyAlign::Center)
                                         ->NodePadding(40)
                                         ->ValueScale(SankeyValueScale::Sqrt);
        for (int i = 0; i < kTslaNodeCount; i++) {
            const TslaNode& node = kTslaNodes[st][i];
            sk->NodeColored(Str(node.name), node.color);
            // The first statement's labels carry the year-over-year change
            // between the value and the name; the second keeps the two
            // default lines.
            Str value = StoryFmt(cx, "$%.2fB", node.value / 1000000000.0);
            if (st == 0) {
                sk->CustomLabel(component::SankeyLabel::New(value));
                if (node.growth != kTslaNoGrowth) {
                    bool up = node.growth >= 0;
                    sk->CustomLabel(
                        component::SankeyLabel::New(
                            StoryFmt(cx, "%s %+.2f%%",
                                     up ? "\xE2\x96\xB2" : "\xE2\x96\xBC",
                                     (double)node.growth))
                            .Color(up ? th.success : th.danger));
                }
                sk->CustomLabel(component::SankeyLabel::New(Str(node.name))
                                    .Color(th.mutedFg));
            } else {
                sk->NodeValue(value);
            }
        }
        for (int i = 0; i < kTslaLinkCount; i++) {
            const TslaLink& link = kTslaLinks[st][i];
            sk->Link(link.source, link.target, link.value);
        }
        sankeyRow->Child(ChartCard(
            cx, StoryFmt(cx, "Sankey Chart - TSLA %s", kTslaPeriods[st]).s,
            sk->IntoEl()->W(kFill)->H(kFill), false));
    }
    page->Child(sankeyRow);
    return page;
}

STORY_PAGE(StoryChart, ChartStory);
