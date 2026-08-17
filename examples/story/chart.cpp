#include "Story.h"

static const float kYs[] = {12, 18, 15, 22, 28, 24, 31, 29, 35, 32, 38, 40};

static El* BarChart(Arena* a) {
    const Theme& th = ThemeNow();
    El* row = Div(a)->FlexRow()->Gap(6)->H(140)->ItemsStart();
    for (int i = 0; i < 12; i++) {
        El* col = Div(a)->FlexCol()->H(140)->W(18)->JustifyEnd();
        col->Child(Div(a)->W(18)->H(kYs[i] * 3)->Bg(th.blue)->Radius(2));
        row->Child(col);
    }
    return row;
}

El* ChartRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    (void)app;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* area = StorySection(a, "Area", "A simple area series.");
    StorySectionAdd(area, component::AreaChart::New(a, kYs, 12)->IntoEl());
    page->Child(area);

    El* bar = StorySection(a, "Bar", "Vertical bars for categorical values.");
    StorySectionAdd(bar, BarChart(a));
    page->Child(bar);

    El* pie = StorySection(a, "Pie", "A compact radial breakdown.");
    El* pieRow = Div(a)->FlexRow()->Gap(16)->ItemsCenter();
    pieRow->Child(
        component::ProgressCircle::New(a)->Value(62)->Size(80)->IntoEl());
    El* legend = Div(a)->FlexCol()->Gap(6);
    legend->Child(StoryTxt(a, StrL("Desktop  62%"), 13, ThemeNow().foreground));
    legend->Child(StoryTxt(a, StrL("Mobile   38%"), 13, ThemeNow().mutedFg));
    pieRow->Child(legend);
    StorySectionAdd(pie, pieRow);
    page->Child(pie);
    return page;
}

void ChartClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryChart, ChartRender, ChartClick);
