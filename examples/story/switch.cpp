#include "Story.h"

static void SetSwitch(StoryApp* app, bool v) {
    app->switchOn = v;
}

El* SwitchRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default",
                           "A control that allows the user to toggle between "
                           "checked and not checked.");
    El* col = Div(a)->FlexCol()->Gap(12);
    col->Child(component::Switch::New(a, StrL("airplane"))
                   ->Label(StrL("Airplane mode"))
                   ->Checked(app->switchOn)
                   ->OnClick(MkFunc1(&SetSwitch, app))
                   ->IntoEl());
    col->Child(component::Switch::New(a, StrL("disabled-sw"))
                   ->Label(StrL("Disabled"))
                   ->Disabled(true)
                   ->IntoEl());
    StorySectionAdd(sec, col);
    page->Child(sec);
    return page;
}

void SwitchClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StorySwitch, SwitchRender, SwitchClick);
