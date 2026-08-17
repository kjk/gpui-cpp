#include "Story.h"

El* LabelRender(StoryApp* app, Arena* a) {
    (void)app;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default",
                           "Primary text with an optional secondary hint.");
    El* col = Div(a)->FlexCol()->Gap(8);
    col->Child(component::Label::New(a, StrL("Display name"))->IntoEl());
    col->Child(component::Label::New(a, StrL("Email"))
                   ->Secondary(StrL("optional"))
                   ->IntoEl());
    col->Child(
        component::Label::New(a, StrL("API token"))->Masked(true)->IntoEl());
    StorySectionAdd(sec, col);
    page->Child(sec);
    return page;
}

void LabelClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryLabel, LabelRender, LabelClick);
