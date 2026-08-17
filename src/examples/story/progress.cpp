#include "Story.h"

El* ProgressRender(StoryApp* app, Arena* a) {
    (void)app;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* bar = StorySection(a, "Progress bar", "Linear completion.");
    El* col = Div(a)->FlexCol()->Gap(12)->W(320);
    col->Child(component::Progress::New(a)->Value(68)->W(280)->IntoEl());
    col->Child(component::Progress::New(a)->Value(32)->W(280)->IntoEl());
    StorySectionAdd(bar, col);
    page->Child(bar);

    El* circ = StorySection(a, "Circle", "Circular progress.");
    El* row = Div(a)->FlexRow()->Gap(16)->ItemsCenter();
    row->Child(component::ProgressCircle::New(a)->Value(68)->Size(56)->IntoEl());
    row->Child(component::ProgressCircle::New(a)->Value(32)->Size(56)->IntoEl());
    StorySectionAdd(circ, row);
    page->Child(circ);
    return page;
}

void ProgressClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryProgress, ProgressRender, ProgressClick);
