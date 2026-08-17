#include "Story.h"

static void SetRadio0(StoryApp* app, bool) {
    app->radioSel = 0;
}
static void SetRadio1(StoryApp* app, bool) {
    app->radioSel = 1;
}
static void SetRadio2(StoryApp* app, bool) {
    app->radioSel = 2;
}

El* RadioRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(
        a, "Default",
        "A set of checkable buttons where only one can be checked at a time.");
    El* col = Div(a)->FlexCol()->Gap(8);
    col->Child(component::Radio::New(a, StrL("r-default"))
                   ->Label(StrL("Default"))
                   ->Checked(app->radioSel == 0)
                   ->OnClick(MkFunc1(&SetRadio0, app))
                   ->IntoEl());
    col->Child(component::Radio::New(a, StrL("r-comfortable"))
                   ->Label(StrL("Comfortable"))
                   ->Checked(app->radioSel == 1)
                   ->OnClick(MkFunc1(&SetRadio1, app))
                   ->IntoEl());
    col->Child(component::Radio::New(a, StrL("r-compact"))
                   ->Label(StrL("Compact"))
                   ->Checked(app->radioSel == 2)
                   ->OnClick(MkFunc1(&SetRadio2, app))
                   ->IntoEl());
    StorySectionAdd(sec, col);
    page->Child(sec);
    return page;
}

void RadioClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryRadio, RadioRender, RadioClick);
