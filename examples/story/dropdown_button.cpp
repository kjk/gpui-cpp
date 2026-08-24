#include "Story.h"

// ButtonAction: the four toggles this page keeps, which both its Options
// dropdown and every dropdown button's own menu drive.
enum {
    DropActDisabled = 3300,
    DropActLoading,
    DropActSelected,
    DropActCompact,
    DropActShadow,
};

struct DropdownButtonStory {
    bool disabled = false;
    bool loading = false;
    bool selected = false;
    bool compact = false;
    StoryToolbarState toolbar;

    static El* Render(DropdownButtonStory* self, Ctx* cx);
};

static void DropAct(DropdownButtonStory* self, Ctx* cx, const ClickEvent*,
                    intptr_t act) {
    switch (act) {
        case DropActDisabled:
            self->disabled = !self->disabled;
            break;
        case DropActLoading:
            self->loading = !self->loading;
            break;
        case DropActSelected:
            self->selected = !self->selected;
            break;
        case DropActCompact:
            self->compact = !self->compact;
            break;
        case DropActShadow:
            // There is no Theme::shadow here to turn on.
            break;
        default:
            StoryToolbarApply(&self->toolbar, nullptr, (int)act);
            break;
    }
    Notify(cx);
}

// The menu reports which row was confirmed; the four rows are the four
// toggles, in order.
static void DropMenuPick(DropdownButtonStory* self, Ctx* cx,
                         const ClickEvent* ev, intptr_t ix) {
    if (ix >= 0 && ix < 4) {
        DropAct(self, cx, ev, DropActDisabled + ix);
    }
}

// The menu every dropdown button carries: the same four checked rows, wired
// to the same four toggles.
static component::PopupMenu* DropMenu(DropdownButtonStory* self, Ctx* cx,
                                      Str id) {
    Entity<PopupMenuState> st = component::PopupMenuStateFor(cx, id);
    if (PopupMenuState* s = st.Get(cx)) {
        s->onConfirm = Listen(cx, &DropMenuPick);
    }
    return component::PopupMenu::New(cx, id, st)
        ->MenuWithCheck(StrL("Disabled"), self->disabled)
        ->MenuWithCheck(StrL("Loading"), self->loading)
        ->MenuWithCheck(StrL("Selected"), self->selected)
        ->MenuWithCheck(StrL("Compact"), self->compact);
}

static component::DropdownButton* DropBtn(DropdownButtonStory* self, Ctx* cx,
                                          Str id, Str label) {
    // Loading and compact are the action button's own: a loading action stays
    // inert while the menu is still there to open. Disabled is the split's.
    component::Button* action = component::Button::New(cx, StrL("btn"))
                                    ->Label(label)
                                    ->Loading(self->loading);
    if (self->compact) {
        action->Compact();
    }
    return component::DropdownButton::New(cx, id)
        ->Button_(action)
        ->Menu(DropMenu(self, cx, StoryFmt(cx, "%s-menu", id)))
        ->WithSize(self->toolbar.size)
        ->Disabled(self->disabled)
        ->Selected(self->selected);
}

El* DropdownButtonStory::Render(DropdownButtonStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    StoryToolbarOpt opts[5] = {
        {"Disabled", self->disabled, DropActDisabled},
        {"Loading", self->loading, DropActLoading},
        {"Selected", self->selected, DropActSelected},
        {"Compact", self->compact, DropActCompact},
        {"Shadow", false, DropActShadow},
    };
    page->Child(StoryToolbarOptions(cx, self, opts, 5, Listen(cx, &DropAct)));

    El* def =
        StorySection(cx, "Default", "A primary action with an attached menu.");
    StorySectionAdd(def,
                    DropBtn(self, cx, StrL("btn0"), StrL("Primary Dropdown"))
                        ->WithVariant(component::ButtonVariant::Primary)
                        ->IntoEl());
    page->Child(def);

    El* out = StorySection(cx, "Outline", nullptr);
    StorySectionAdd(
        out, DropBtn(self, cx, StrL("btn-outline"), StrL("Outline Dropdown"))
                 ->WithVariant(component::ButtonVariant::Danger)
                 ->Outline()
                 ->IntoEl());
    page->Child(out);

    El* ghost = StorySection(cx, "Ghost", nullptr);
    StorySectionAdd(ghost,
                    DropBtn(self, cx, StrL("btn-ghost"), StrL("Ghost Dropdown"))
                        ->WithVariant(component::ButtonVariant::Ghost)
                        ->IntoEl());
    page->Child(ghost);
    return page;
}

STORY_PAGE(StoryDropdownButton, DropdownButtonStory);
