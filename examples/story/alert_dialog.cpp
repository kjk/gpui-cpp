#include "Story.h"

struct AlertDialogStory {
    bool alertOpen = false;
    int selB = -1;

    static El* Render(AlertDialogStory* self, Ctx* cx);
    static void Click(AlertDialogStory* self, Ctx* cx, int id);
};

static void CloseAlert(AlertDialogStory* self) {
    self->alertOpen = false;
}

El* AlertDialogStory::Render(AlertDialogStory* self, Ctx* cx) {
    WinSize size = WindowSize(cx->win);
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* def = StorySection(
        cx, "Default",
        "A modal dialog that interrupts the user with important content.");
    StorySectionAdd(def, component::Button::New(cx, StrL("open-alert"))
                             ->Label(StrL("Show Dialog"))
                             ->Outline()
                             ->IntoEl());
    page->Child(def);

    El* dest = StorySection(cx, "Destructive", nullptr);
    StorySectionAdd(dest, component::Button::New(cx, StrL("open-alert-danger"))
                              ->Label(StrL("Delete project"))
                              ->Danger()
                              ->IntoEl());
    page->Child(dest);

    El* none = StorySection(cx, "Without title", nullptr);
    StorySectionAdd(none, component::Button::New(cx, StrL("open-alert-notitle"))
                              ->Label(StrL("Confirm"))
                              ->Outline()
                              ->IntoEl());
    page->Child(none);

    if (self->alertOpen) {
        component::Dialog* d = component::Dialog::New(cx)
                                   ->Open(true)
                                   ->OnClose(MkFunc0(&CloseAlert, self))
                                   ->OnOk(MkFunc0(&CloseAlert, self));
        if (self->selB == 1) {
            d->Title(StrL("Delete project?"))
                ->Description(StrL("This permanently deletes Acme Studio and "
                                   "all of its data."));
        } else if (self->selB == 2) {
            d->Description(StrL("Continue without a title on this confirm?"));
        } else {
            d->Title(StrL("Are you sure?"))
                ->Description(StrL("This action cannot be undone."));
        }
        page->Child(d->IntoEl(size));
    }
    return page;
}

void AlertDialogStory::Click(AlertDialogStory* self, Ctx* cx, int id) {
    (void)cx;
    if (id == HashClickId(StrL("open-alert"))) {
        self->alertOpen = true;
        self->selB = 0;
    } else if (id == HashClickId(StrL("open-alert-danger"))) {
        self->alertOpen = true;
        self->selB = 1;
    } else if (id == HashClickId(StrL("open-alert-notitle"))) {
        self->alertOpen = true;
        self->selB = 2;
    }
}

STORY_PAGE(StoryAlertDialog, AlertDialogStory);
