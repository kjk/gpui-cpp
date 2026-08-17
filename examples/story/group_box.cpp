#include "Story.h"

El* GroupBoxRender(StoryApp* app, Arena* a) {
    (void)app;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(
        a, "Default", "A container that groups related content with a title.");
    StorySectionAdd(
        sec, component::GroupBox::New(a, StrL("Appearance"))
                 ->Child(StoryTxt(
                     a, StrL("Theme, radius, and density live together."), 13,
                     ThemeNow().mutedFg))
                 ->IntoEl());
    page->Child(sec);
    return page;
}

void GroupBoxClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryGroupBox, GroupBoxRender, GroupBoxClick);
