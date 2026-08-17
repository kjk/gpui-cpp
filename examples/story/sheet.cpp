#include "Story.h"

enum { ClickStorySheetOpen = 2720 };

static void CloseSheet(StoryApp* app) {
    app->sheetOpen = false;
}

El* SheetRender(StoryApp* app, Arena* a, WinSize size) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default", "A panel that slides in from the edge of the screen.");
    StorySectionAdd(sec, component::Button::New(a, StrL("open-sheet-story"))->Label(StrL("Open settings"))->Outline()->IntoEl());
    page->Child(sec);
    if (app->sheetOpen) {
        page->Child(component::Sheet::New(a)
                        ->Open(true)
                        ->Title(StrL("Settings"))
                        ->Body(StoryTxt(a, StrL("Workspace preferences for your team."), 13, ThemeNow().mutedFg))
                        ->OnClose(MkFunc0(&CloseSheet, app))
                        ->IntoEl(size));
    }
    return page;
}

void SheetClick(StoryApp* app, int id) {
    if (id == ClickStorySheetOpen || id == HashClickId(StrL("open-sheet-story"))) {
        app->sheetOpen = true;
    }
}

STORY_PAGE_SZ(StorySheet, SheetRender, SheetClick);
