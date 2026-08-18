#include "Story.h"

struct ChartStory {
    static El* Render(ChartStory* self, Ctx* cx);
    static void Click(ChartStory* self, Ctx* cx, int id);
};

static const float kYs[] = {12, 18, 15, 22, 28, 24, 31, 29, 35, 32, 38, 40};

static El* BarChart(Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
    El* row = Div(a)->FlexRow()->Gap(6)->H(140)->ItemsStart();
    for (int i = 0; i < 12; i++) {
        El* col = Div(a)->FlexCol()->H(140)->W(18)->JustifyEnd();
        col->Child(Div(a)->W(18)->H(kYs[i] * 3)->Bg(th.blue)->Radius(2));
        row->Child(col);
    }
    return row;
}

El* ChartStory::Render(ChartStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* area = StorySection(cx, "Area", "A simple area series.");
    StorySectionAdd(area, component::AreaChart::New(cx, kYs, 12)->IntoEl());
    page->Child(area);

    El* bar = StorySection(cx, "Bar", "Vertical bars for categorical values.");
    StorySectionAdd(bar, BarChart(cx));
    page->Child(bar);

    El* pie = StorySection(cx, "Pie", "A compact radial breakdown.");
    El* pieRow = Div(a)->FlexRow()->Gap(16)->ItemsCenter();
    pieRow->Child(
        component::ProgressCircle::New(cx)->Value(62)->Size(80)->IntoEl());
    El* legend = Div(a)->FlexCol()->Gap(6);
    legend
        ->Child(StoryTxt(cx, StrL("Desktop  62%"), 13, ThemeNow().foreground));
    legend->Child(StoryTxt(cx, StrL("Mobile   38%"), 13, ThemeNow().mutedFg));
    pieRow->Child(legend);
    StorySectionAdd(pie, pieRow);
    page->Child(pie);
    return page;
}

void ChartStory::Click(ChartStory* self, Ctx* cx, int id) {
    (void)cx;
    (void)id;
}

STORY_PAGE(StoryChart, ChartStory);
