#include "Story.h"

enum { ClickStoryDlgOpen = 2700 };

static void CloseDlg(StoryApp* app) {
    app->dialogOpen = false;
}

El* DialogRender(StoryApp* app, Arena* a, WinSize size) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default", "A window overlaid on the primary window.");
    StorySectionAdd(sec, component::Button::New(a, StrL("open-dlg"))
                             ->Label(StrL("Edit profile"))
                             ->Primary()
                             ->IntoEl());
    page->Child(sec);
    if (app->dialogOpen) {
        page->Child(component::Dialog::New(a)
                        ->Open(true)
                        ->Title(StrL("Edit profile"))
                        ->Description(StrL("Update the public details shown on your profile."))
                        ->OnClose(MkFunc0(&CloseDlg, app))
                        ->OnOk(MkFunc0(&CloseDlg, app))
                        ->IntoEl(size));
    }
    return page;
}

void DialogClick(StoryApp* app, int id) {
    if (id == ClickStoryDlgOpen || id == HashClickId(StrL("open-dlg"))) {
        app->dialogOpen = true;
    }
}

STORY_PAGE_SZ(StoryDialog, DialogRender, DialogClick);
