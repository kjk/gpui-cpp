#include "Story.h"

struct DropdownButtonStory {
    int selectIx = 0;
    bool selectOpen = false;
    StoryToolbarState toolbar;

    static El* Render(DropdownButtonStory* self, Ctx* cx);
    static void Click(DropdownButtonStory* self, Ctx* cx, int id);
};

enum {
    ClickDropDefault = 2750,
    ClickDropOutline,
    ClickDropGhost
};

static El* DropMenu(Ctx* cx) {
    Arena* a = cx->a;
    return component::Menu::New(cx)
        ->Item(StrL("Disabled"))
        ->Item(StrL("Loading"))
        ->Item(StrL("Selected"))
        ->Item(StrL("Compact"))
        ->IntoEl();
}

static El* DropBlock(Ctx* cx, DropdownButtonStory* self, int which,
                     component::Button* btn) {
    Arena* a = cx->a;
    El* col = Div(a)->FlexCol()->Gap(4);
    col->Child(btn->DropdownCaret()->IntoEl()->Click(2750 + which));
    if (self->selectIx == which && self->selectOpen) {
        col->Child(DropMenu(cx));
    }
    return col;
}

El* DropdownButtonStory::Render(DropdownButtonStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(cx, &self->toolbar));

    El* def =
        StorySection(cx, "Default", "A primary action with an attached menu.");
    StorySectionAdd(def, DropBlock(cx, self, 0,
                                   component::Button::New(cx, StrL("btn0"))
                                       ->Label(StrL("Primary Dropdown"))
                                       ->Primary()
                                       ->WithSize(self->toolbar.size)));
    page->Child(def);

    El* out = StorySection(cx, "Outline", nullptr);
    StorySectionAdd(out,
                    DropBlock(cx, self, 1,
                              component::Button::New(cx, StrL("btn-outline"))
                                  ->Label(StrL("Outline Dropdown"))
                                  ->Danger()
                                  ->Outline()
                                  ->WithSize(self->toolbar.size)));
    page->Child(out);

    El* ghost = StorySection(cx, "Ghost", nullptr);
    StorySectionAdd(ghost,
                    DropBlock(cx, self, 2,
                              component::Button::New(cx, StrL("btn-ghost"))
                                  ->Label(StrL("Ghost Dropdown"))
                                  ->Ghost()
                                  ->WithSize(self->toolbar.size)));
    page->Child(ghost);
    return page;
}

void DropdownButtonStory::Click(DropdownButtonStory* self, Ctx* cx, int id) {
    if (StoryToolbarClick(&self->toolbar, id)) {
        return;
    }
    (void)cx;
    int which = -1;
    if (id == ClickDropDefault || id == HashClickId(StrL("btn0"))) {
        which = 0;
    } else if (id == ClickDropOutline ||
               id == HashClickId(StrL("btn-outline"))) {
        which = 1;
    } else if (id == ClickDropGhost || id == HashClickId(StrL("btn-ghost"))) {
        which = 2;
    }
    if (which < 0) {
        return;
    }
    if (self->selectOpen && self->selectIx == which) {
        self->selectOpen = false;
    } else {
        self->selectOpen = true;
        self->selectIx = which;
    }
}

STORY_PAGE(StoryDropdownButton, DropdownButtonStory);
