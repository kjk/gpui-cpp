#include "Story.h"

static void CloseAlert(StoryApp* app) {
    app->alertOpen = false;
}

El* AlertDialogRender(StoryApp* app, Arena* a, WinSize size) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* def = StorySection(
        a, "Default",
        "A modal dialog that interrupts the user with important content.");
    StorySectionAdd(def, component::Button::New(a, StrL("open-alert"))
                             ->Label(StrL("Show Dialog"))
                             ->Outline()
                             ->IntoEl());
    page->Child(def);

    El* dest = StorySection(a, "Destructive", nullptr);
    StorySectionAdd(dest, component::Button::New(a, StrL("open-alert-danger"))
                              ->Label(StrL("Delete project"))
                              ->Danger()
                              ->IntoEl());
    page->Child(dest);

    El* none = StorySection(a, "Without title", nullptr);
    StorySectionAdd(none, component::Button::New(a, StrL("open-alert-notitle"))
                              ->Label(StrL("Confirm"))
                              ->Outline()
                              ->IntoEl());
    page->Child(none);

    if (app->alertOpen) {
        component::Dialog* d = component::Dialog::New(a)
                                   ->Open(true)
                                   ->OnClose(MkFunc0(&CloseAlert, app))
                                   ->OnOk(MkFunc0(&CloseAlert, app));
        if (app->selB == 1) {
            d->Title(StrL("Delete project?"))
                ->Description(StrL("This permanently deletes Acme Studio and "
                                   "all of its data."));
        } else if (app->selB == 2) {
            d->Description(StrL("Continue without a title on this confirm?"));
        } else {
            d->Title(StrL("Are you sure?"))
                ->Description(StrL("This action cannot be undone."));
        }
        page->Child(d->IntoEl(size));
    }
    return page;
}

void AlertDialogClick(StoryApp* app, int id) {
    if (id == HashClickId(StrL("open-alert"))) {
        app->alertOpen = true;
        app->selB = 0;
    } else if (id == HashClickId(StrL("open-alert-danger"))) {
        app->alertOpen = true;
        app->selB = 1;
    } else if (id == HashClickId(StrL("open-alert-notitle"))) {
        app->alertOpen = true;
        app->selB = 2;
    }
}

STORY_PAGE_SZ(StoryAlertDialog, AlertDialogRender, AlertDialogClick);
