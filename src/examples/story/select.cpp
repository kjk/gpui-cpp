#include "Story.h"

static void ToggleSel(StoryApp* app) {
    app->selectOpen = !app->selectOpen;
}
static void PickSel(StoryApp* app, int i) {
    app->selectIx = i;
    app->selectOpen = false;
}

El* SelectRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default", "Displays a list of options for the user to pick from.");
    StorySectionAdd(sec, component::Select::New(a, StrL("framework"))
                             ->Option(StrL("GPUI"))
                             ->Option(StrL("React"))
                             ->Option(StrL("SwiftUI"))
                             ->Option(StrL("Vue"))
                             ->Selected(app->selectIx)
                             ->Open(app->selectOpen)
                             ->OnToggle(MkFunc0(&ToggleSel, app))
                             ->OnChange(MkFunc1(&PickSel, app))
                             ->IntoEl());
    page->Child(sec);
    return page;
}

void SelectClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StorySelect, SelectRender, SelectClick);
