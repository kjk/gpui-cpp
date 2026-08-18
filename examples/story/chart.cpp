#include "Story.h"

struct ChartStory {
    static El* Render(ChartStory* self, Ctx* cx);
};

// The Rust story generates 90 days of random device counts; ours uses a
// deterministic stand-in with the same shape.
static float SeriesAt(int i, int seed) {
    unsigned h = (unsigned)(i * 2654435761u + seed * 40503u);
    h ^= h >> 13;
    return 20.f + (float)(h % 100);
}

// chart_container(): a 400px card with the title, the range, the chart and
// two lines of commentary.
static El* ChartCard(Ctx* cx, const char* title, El* chart, bool center) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* card = Div(a)
                   ->FlexCol()
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

El* ChartStory::Render(ChartStory*, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* page = Div(a)->FlexCol()->Gap(16)->W(kFill);

    // Two stacked area series, desktop over mobile.
    const int kDays = 90;
    float* desktop = (float*)Alloc(a, (int)sizeof(float) * kDays);
    float* mobile = (float*)Alloc(a, (int)sizeof(float) * kDays);
    for (int i = 0; i < kDays; i++) {
        desktop[i] = SeriesAt(i, 1);
        mobile[i] = SeriesAt(i, 2) * 0.6f;
    }
    // The x axis is labelled by date, every eighth point.
    const char** labels = (const char**)Alloc(a, (int)sizeof(char*) * kDays);
    static const char* kMonths[] = {"Apr", "May", "Jun"};
    for (int i = 0; i < kDays; i++) {
        int m = i / 30;
        labels[i] =
            StrDup(a, fmt("%s %d", Str(kMonths[m < 3 ? m : 2]), i % 30 + 1)).s;
    }
    El* areaBox = Div(a)->W(kFill)->H(kFill);
    El* area1 = component::AreaChart::New(cx, desktop, kDays)
                    ->Stroke(th.blue)
                    ->Fill(RgbaOpacity(th.blue, 0.4f))
                    ->Labels(labels)
                    ->TickMargin(8)
                    ->IntoEl();
    El* area2 =
        component::AreaChart::New(cx, mobile, kDays)
            ->Stroke(RgbaMix(th.blue, th.background, 0.4f))
            ->Fill(RgbaOpacity(RgbaMix(th.blue, th.background, 0.4f), 0.4f))
            ->Overlay()
            ->IntoEl();
    areaBox->Child(area1->W(kFill)->H(kFill));
    areaBox->Child(area2->Absolute()->Left(0)->Top(0)->W(kFill)->H(kFill));
    page->Child(ChartCard(cx, "Area Chart - Stacked", areaBox, false));

    // The pie row: a full pie and a donut, each of six slices.
    static const float kValues[] = {186, 305, 237, 173, 209, 214};
    El* pieRow = Div(a)->FlexRow()->W(kFill)->Gap(16)->FlexWrap();
    component::PieChart* pie = component::PieChart::New(cx)->OuterRadius(100);
    component::PieChart* donut =
        component::PieChart::New(cx)->OuterRadius(100)->InnerRadius(60);
    for (int i = 0; i < 6; i++) {
        // The slices step from the chart color toward the background.
        Rgba c = RgbaMix(th.blue, th.background, (float)i * 0.08f);
        pie->Slice(kValues[i], c);
        donut->Slice(kValues[i], c, (float)i * 4.f);
    }
    pieRow->Child(ChartCard(cx, "Pie Chart", pie->IntoEl(), true));
    pieRow->Child(ChartCard(cx, "Pie Chart - Donut", donut->IntoEl(), true));
    page->Child(pieRow);
    return page;
}

STORY_PAGE(StoryChart, ChartStory);
