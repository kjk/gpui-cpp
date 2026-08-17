#include "Story.h"

El* ChartRender(StoryApp* app, Arena* a) {
    (void)app;
    static const float ys[] = {12, 18, 15, 22, 28, 24, 31, 29, 35, 32, 38, 40};
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Area", "A simple area series.");
    StorySectionAdd(sec, component::AreaChart::New(a, ys, 12)->IntoEl());
    page->Child(sec);
    return page;
}

void ChartClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryChart, ChartRender, ChartClick);
