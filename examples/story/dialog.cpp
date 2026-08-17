#include "Story.h"

static void CloseDlg(StoryApp* app) {
    app->dialogOpen = false;
}

El* DialogRender(StoryApp* app, Ctx* cx, WinSize size) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* def =
        StorySection(a, "Default", "A window overlaid on the primary window.");
    StorySectionAdd(def, component::Button::New(a, StrL("open-dlg"))
                             ->Label(StrL("Edit profile"))
                             ->Primary()
                             ->IntoEl());
    page->Child(def);

    El* none = StorySection(a, "Without title", nullptr);
    StorySectionAdd(none, component::Button::New(a, StrL("open-dlg-notitle"))
                              ->Label(StrL("Open"))
                              ->Outline()
                              ->IntoEl());
    page->Child(none);

    El* acts = StorySection(a, "Custom actions", nullptr);
    StorySectionAdd(acts, component::Button::New(a, StrL("open-dlg-custom"))
                              ->Label(StrL("Share"))
                              ->Secondary()
                              ->IntoEl());
    page->Child(acts);

    if (app->dialogOpen) {
        component::Dialog* d = component::Dialog::New(a)
                                   ->Open(true)
                                   ->OnClose(MkFunc0(&CloseDlg, app))
                                   ->OnOk(MkFunc0(&CloseDlg, app));
        if (app->selB == 1) {
            d->Description(
                StrL("This dialog has no title, only a short message."));
        } else if (app->selB == 2) {
            d->Title(StrL("Share workspace"))
                ->Description(StrL("Anyone with the link can view this file."));
        } else {
            d->Title(StrL("Edit profile"))
                ->Description(
                    StrL("Update the public details shown on your profile."));
        }
        page->Child(d->IntoEl(size));
    }
    return page;
}

void DialogClick(StoryApp* app, int id) {
    if (id == HashClickId(StrL("open-dlg"))) {
        app->dialogOpen = true;
        app->selB = 0;
    } else if (id == HashClickId(StrL("open-dlg-notitle"))) {
        app->dialogOpen = true;
        app->selB = 1;
    } else if (id == HashClickId(StrL("open-dlg-custom"))) {
        app->dialogOpen = true;
        app->selB = 2;
    }
}

STORY_PAGE_SZ(StoryDialog, DialogRender, DialogClick);
