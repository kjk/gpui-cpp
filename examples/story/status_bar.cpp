#include "Story.h"

El* StatusBarRender(StoryApp* app, Arena* a) {
    (void)app;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(
        a, "Default",
        "A status bar that typically sits at the bottom of the window.");
    StorySectionAdd(sec, component::StatusBar::New(a)
                             ->Left(StrL("Ready"))
                             ->Right(StrL("Ln 12, Col 4"))
                             ->IntoEl());
    page->Child(sec);
    return page;
}

void StatusBarClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryStatusBar, StatusBarRender, StatusBarClick);
