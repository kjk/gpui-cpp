#include "Story.h"

struct DialogStory {
    bool dialogOpen = false;
    int selB = -1;

    static El* Render(DialogStory* self, Ctx* cx);
    static void Click(DialogStory* self, Ctx* cx, int id);
};

static void OpenDialog(DialogStory* self, Ctx* cx, const ClickEvent*,
                       intptr_t variant) {
    self->dialogOpen = true;
    self->selB = (int)variant;
    Notify(cx);
}

static void CloseDlg(DialogStory* self, Ctx* cx, const ClickEvent*) {
    self->dialogOpen = false;
}

El* DialogStory::Render(DialogStory* self, Ctx* cx) {
    WinSize size = WindowSize(cx->win);
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* def =
        StorySection(cx, "Default", "A window overlaid on the primary window.");
    StorySectionAdd(def, component::Button::New(cx, StrL("open-dlg"))
                             ->OnClick(Listen(cx, &OpenDialog, 0))
                             ->Label(StrL("Edit profile"))
                             ->Primary()
                             ->IntoEl());
    page->Child(def);

    El* none = StorySection(cx, "Without title", nullptr);
    StorySectionAdd(none, component::Button::New(cx, StrL("open-dlg-notitle"))
                              ->OnClick(Listen(cx, &OpenDialog, 1))
                              ->Label(StrL("Open"))
                              ->Outline()
                              ->IntoEl());
    page->Child(none);

    El* acts = StorySection(cx, "Custom actions", nullptr);
    StorySectionAdd(acts, component::Button::New(cx, StrL("open-dlg-custom"))
                              ->OnClick(Listen(cx, &OpenDialog, 2))
                              ->Label(StrL("Share"))
                              ->Secondary()
                              ->IntoEl());
    page->Child(acts);

    if (self->dialogOpen) {
        component::Dialog* d = component::Dialog::New(cx)
                                   ->Open(true)
                                   ->OnClose(Listen(cx, &CloseDlg))
                                   ->OnOk(Listen(cx, &CloseDlg));
        if (self->selB == 1) {
            d->Description(
                StrL("This dialog has no title, only a short message."));
        } else if (self->selB == 2) {
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

void DialogStory::Click(DialogStory* self, Ctx* cx, int id) {
    (void)self;
    (void)cx;
    (void)id;
}

STORY_PAGE(StoryDialog, DialogStory);
