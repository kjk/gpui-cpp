#include "Story.h"

struct AlertDialogStory {
    bool alertOpen = false;
    int selB = -1;

    static El* Render(AlertDialogStory* self, Ctx* cx);
    static void OnKey(AlertDialogStory* self, Ctx* cx, const KeyEvent* ev);
};

static void OpenAlert(AlertDialogStory* self, Ctx* cx, const ClickEvent*,
                      intptr_t variant) {
    self->alertOpen = true;
    self->selB = (int)variant;
    Notify(cx);
}

static void CloseAlert(AlertDialogStory* self, Ctx* cx, const ClickEvent*) {
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
                             ->OnClick(Listen(cx, &OpenAlert, 0))
                             ->Label(StrL("Show Dialog"))
                             ->Outline()
                             ->IntoEl());
    page->Child(def);

    El* dest = StorySection(cx, "Destructive", nullptr);
    StorySectionAdd(dest, component::Button::New(cx, StrL("open-alert-danger"))
                              ->OnClick(Listen(cx, &OpenAlert, 1))
                              ->Label(StrL("Delete project"))
                              ->Danger()
                              ->IntoEl());
    page->Child(dest);

    El* none = StorySection(cx, "Without title", nullptr);
    StorySectionAdd(none, component::Button::New(cx, StrL("open-alert-notitle"))
                              ->OnClick(Listen(cx, &OpenAlert, 2))
                              ->Label(StrL("Confirm"))
                              ->Outline()
                              ->IntoEl());
    page->Child(none);

    if (self->alertOpen) {
        component::Dialog* d = component::Dialog::New(cx)
                                   ->Open(true)
                                   ->OnClose(Listen(cx, &CloseAlert))
                                   ->OnOk(Listen(cx, &CloseAlert));
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

// Esc closes what this page has open, like an overlay dismiss.
void AlertDialogStory::OnKey(AlertDialogStory* self, Ctx* cx,
                             const KeyEvent* ev) {
    if (ev->vk != VK_ESCAPE) {
        return;
    }
    self->alertOpen = false;
    Notify(cx);
}

STORY_PAGE_KEYS(StoryAlertDialog, AlertDialogStory);
