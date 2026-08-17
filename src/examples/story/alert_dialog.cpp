#include "Story.h"

enum { ClickStoryAlertOpen = 2710 };

static void CloseAlert(StoryApp* app) {
    app->alertOpen = false;
}

El* AlertDialogRender(StoryApp* app, Arena* a, WinSize size) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default", "A modal dialog that interrupts the user with important content.");
    StorySectionAdd(sec, component::Button::New(a, StrL("open-alert"))->Label(StrL("Delete project"))->Danger()->IntoEl());
    page->Child(sec);
    if (app->alertOpen) {
        page->Child(component::Dialog::New(a)
                        ->Open(true)
                        ->Title(StrL("Delete project?"))
                        ->Description(StrL("This permanently deletes Acme Studio and all of its data."))
                        ->OnClose(MkFunc0(&CloseAlert, app))
                        ->OnOk(MkFunc0(&CloseAlert, app))
                        ->IntoEl(size));
    }
    return page;
}

void AlertDialogClick(StoryApp* app, int id) {
    if (id == ClickStoryAlertOpen || id == HashClickId(StrL("open-alert"))) {
        app->alertOpen = true;
    }
}

STORY_PAGE_SZ(StoryAlertDialog, AlertDialogRender, AlertDialogClick);
