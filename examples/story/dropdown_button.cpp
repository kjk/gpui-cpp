#include "Story.h"

enum {
    DropOptDisabled = ToolbarOptMultiple,
    DropOptLoading = ToolbarOptIcon,
    DropOptSelected = ToolbarOptDisabled,
    DropOptCompact = ToolbarOptBordered,
    DropOptShadow = ToolbarOptHorizontal
};

struct DropdownButtonStory {
    int selectIx = 0;
    bool selectOpen = false;
    bool disabled = false;
    bool loading = false;
    bool selected = false;
    bool compact = false;
    bool shadow = false;
    StoryToolbarState toolbar;

    static El* Render(DropdownButtonStory* self, Ctx* cx);
    static void OnKey(DropdownButtonStory* self, Ctx* cx, const KeyEvent* ev);
};

static void ToggleDrop(DropdownButtonStory* self, Ctx* cx, const ClickEvent*,
                       intptr_t which) {
    if (self->selectOpen && self->selectIx == (int)which) {
        self->selectOpen = false;
    } else {
        self->selectOpen = true;
        self->selectIx = (int)which;
    }
    Notify(cx);
}

static void DropToolbarAct(DropdownButtonStory* self, Ctx* cx,
                           const ClickEvent*, intptr_t act) {
    switch (act) {
        case DropOptDisabled:
            self->disabled = !self->disabled;
            break;
        case DropOptLoading:
            self->loading = !self->loading;
            break;
        case DropOptSelected:
            self->selected = !self->selected;
            break;
        case DropOptCompact:
            self->compact = !self->compact;
            break;
        case DropOptShadow:
            self->shadow = !self->shadow;
            break;
        default:
            StoryToolbarApply(&self->toolbar, nullptr, (int)act);
            break;
    }
    Notify(cx);
}

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
    btn->Disabled(self->disabled)
        ->Loading(self->loading)
        ->Selected(self->selected);
    if (self->compact) {
        btn->Compact();
    }
    col->Child(btn->DropdownCaret()->IntoEl()->Click(2750 + which));
    if (self->selectIx == which && self->selectOpen) {
        col->Child(DropMenu(cx));
    }
    return col;
}

El* DropdownButtonStory::Render(DropdownButtonStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    StoryToolbarOpt opts[5] = {
        {"Disabled", self->disabled, DropOptDisabled},
        {"Loading", self->loading, DropOptLoading},
        {"Selected", self->selected, DropOptSelected},
        {"Compact", self->compact, DropOptCompact},
        {"Shadow", self->shadow, DropOptShadow},
    };
    page->Child(
        StoryToolbarOptions(cx, self, opts, 5, Listen(cx, &DropToolbarAct)));

    El* def =
        StorySection(cx, "Default", "A primary action with an attached menu.");
    StorySectionAdd(def, DropBlock(cx, self, 0,
                                   component::Button::New(cx, StrL("btn0"))
                                       ->OnClick(Listen(cx, &ToggleDrop, 0))
                                       ->Label(StrL("Primary Dropdown"))
                                       ->Primary()
                                       ->WithSize(self->toolbar.size)));
    page->Child(def);

    El* out = StorySection(cx, "Outline", nullptr);
    StorySectionAdd(out,
                    DropBlock(cx, self, 1,
                              component::Button::New(cx, StrL("btn-outline"))
                                  ->OnClick(Listen(cx, &ToggleDrop, 1))
                                  ->Label(StrL("Outline Dropdown"))
                                  ->Danger()
                                  ->Outline()
                                  ->WithSize(self->toolbar.size)));
    page->Child(out);

    El* ghost = StorySection(cx, "Ghost", nullptr);
    StorySectionAdd(ghost,
                    DropBlock(cx, self, 2,
                              component::Button::New(cx, StrL("btn-ghost"))
                                  ->OnClick(Listen(cx, &ToggleDrop, 2))
                                  ->Label(StrL("Ghost Dropdown"))
                                  ->Ghost()
                                  ->WithSize(self->toolbar.size)));
    page->Child(ghost);
    return page;
}

// Esc closes what this page has open, like an overlay dismiss.
void DropdownButtonStory::OnKey(DropdownButtonStory* self, Ctx* cx,
                                const KeyEvent* ev) {
    if (ev->vk != KeyEscape) {
        return;
    }
    self->selectOpen = false;
    Notify(cx);
}

STORY_PAGE_KEYS(StoryDropdownButton, DropdownButtonStory);
