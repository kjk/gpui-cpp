#include "Story.h"
#include "ChartFixtures.h"

struct ChartStory {
    static El* Render(ChartStory* self, Ctx* cx);
};

// chart_container(): a 400px card with the title, the range, the chart and
// two lines of commentary.
static El* ChartCard(Ctx* cx, const char* title, El* chart, bool center) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* card = Div(a)
                   ->FlexCol()
                   // flex_1. In Rust a wrapping row breaks its lines on each
                   // card's min-content width, so two of these land per line;
                   // layout here shares the row out first and wraps on what
                   // is left, so more fit and the chart inside overflows.
                   ->Grow()
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
    El* body = Div(a)->Grow()->W(kFill)->PadY(16)->FlexRow();
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
    const Theme& th = cx->theme();
    // `let color = cx.theme().chart_3`, which every pie and mixed bar tints
    // by its own alpha.
    Rgba color = th.chart3;
    El* page = Div(a)->FlexCol()->Gap(16)->W(kFill);

    // Area Chart - Stacked: desktop over mobile, both from daily-devices.json.
    El* areaBox = Div(a)->W(kFill)->H(kFill);
    El* area1 = component::AreaChart::New(cx, kDailyDesktop, kDailyDeviceCount)
                    ->Tooltip(StrL("Desktop"))
                    ->Stroke(th.chart1)
                    ->Fill(RgbaOpacity(th.chart1, 0.4f),
                           RgbaOpacity(th.background, 0.3f))
                    ->Labels(kDailyDate)
                    ->TickMargin(8)
                    ->IntoEl();
    El* area2 = component::AreaChart::New(cx, kDailyMobile, kDailyDeviceCount)
                    ->Stroke(th.chart2)
                    ->Fill(RgbaOpacity(th.chart2, 0.4f),
                           RgbaOpacity(th.background, 0.3f))
                    ->Overlay()
                    ->IntoEl();
    areaBox->Child(area1->W(kFill)->H(kFill));
    areaBox->Child(area2->Absolute()->Left(0)->Top(0)->W(kFill)->H(kFill));
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
        labelled->Slice(kMonthlyDesktop[i], c);
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
    // Radar Chart - Lines Only: max_value(400) and no fill under the ring.
    radarRow->Child(ChartCard(
        cx, "Radar Chart - Lines Only",
        component::RadarChart::New(cx, kRadarDesktop, kRadarDeviceCount)
            ->Labels(kRadarMonth)
            ->Stroke(th.chart3)
            ->Fill(Rgba8(0, 0, 0, 0))
            ->Domain(0, 400)
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
    // Bar Chart - Rounded corners: corner_radii(px(8.)).
    barRow->Child(ChartCard(
        cx, "Bar Chart - Rounded corners",
        component::BarChart::New(cx, kMonthlyDesktop, kMonthlyDeviceCount)
            ->Fill(th.chart1)
            ->Labels(kMonthlyMonth)
            ->TickMargin(1)
            ->Radius(8)
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
    page->Child(lineRow);
    page->Child(component::Separator::Horizontal(cx)->IntoEl());

    // A sankey: where a week of energy comes from and what it goes to, which
    // is the shape the d3 example uses. Rust's is a TSLA income statement out
    // of a fixture with its own colors per node.
    component::SankeyChart* sankey = component::SankeyChart::New(cx)
                                         ->Node(StrL("Coal"))
                                         ->Node(StrL("Gas"))
                                         ->Node(StrL("Wind"))
                                         ->Node(StrL("Solar"))
                                         ->Node(StrL("Grid"))
                                         ->Node(StrL("Homes"))
                                         ->Node(StrL("Industry"))
                                         ->Node(StrL("Transport"))
                                         ->Link(0, 4, 30)
                                         ->Link(1, 4, 25)
                                         ->Link(2, 4, 20)
                                         ->Link(3, 4, 15)
                                         ->Link(4, 5, 40)
                                         ->Link(4, 6, 32)
                                         ->Link(4, 7, 18)
                                         ->ShowValues()
                                         ->NodeCornerRadius(2);
    page->Child(ChartCard(cx, "Sankey Chart",
                          sankey->IntoEl()->W(kFill)->H(kFill), false));
    return page;
}

STORY_PAGE(StoryChart, ChartStory);
