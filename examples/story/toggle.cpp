#include "Story.h"

struct ToggleStory {
    int toggleSel = 1;
    bool toggles[10] = {};
    StoryToolbarState toolbar;

    static El* Render(ToggleStory* self, Ctx* cx);
};

static void OnToggle(ToggleStory* self, Ctx* cx, const ClickEvent*,
                     intptr_t slot) {
    if (slot < 0) {
        self->toggleSel = self->toggleSel == 1 ? 0 : 1;
    } else if (slot < 10) {
        self->toggles[slot] = !self->toggles[slot];
    }
    Notify(cx);
}

// `seg` is where this chip sits in a segmented group: -1 for a chip on its
// own, 0 for the first, 1 for a middle one and 2 for the last. A segmented
// group is a row of outline toggles that drop their left edge after the
// first, so the border between two of them is one line rather than two, and
// only the ends are rounded (button/toggle.rs ToggleGroup::render).
static El* ToggleChip(Ctx* cx, Listener onToggle, int slot, const char* label,
                      IconName icon, bool on, bool outline, int seg = -1) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    // The toggle takes the press itself; the page still needs to know which
    // chip it was, so the slot rides on the listener the way Rust captures it
    // in the closure.
    El* t = Toggle::New(cx, StrDup(a, fmt("tog-%d", slot)), on, false,
                        ListenerArg(onToggle, slot))
                ->H(28)
                ->PadX(label ? 10.f : 8.f)
                ->ItemsCenter()
                ->JustifyCenter()
                ->Gap(6);
    // A chip in a group takes its rounding from the group's own clip, the
    // way a joined Button does.
    t->Radius(seg < 0 ? th.radius : 0.f);
    if (outline) {
        if (seg <= 0) {
            t->Border(1, th.border);
        } else {
            t->BorderT(1, th.border)
                ->BorderR(1, th.border)
                ->BorderB(1, th.border);
        }
    }
    if (on) {
        t->Bg(th.tokens.accent);
    } else {
        t->HoverBg(th.tokens.muted);
    }
    if (icon != IconName::None) {
        t->Child(IconEl(a, icon, 14)->Fg(th.foreground));
    }
    if (label) {
        // Toggle is text_xs at XSmall and text_sm at Small; Medium and Large
        // name no size and read at the base (button/toggle.rs).
        t->Child(StoryTxt(cx, Str(label), 16, th.foreground));
    }
    return t;
}

El* ToggleStory::Render(ToggleStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    Listener onToggle = Listen(cx, &OnToggle);
    El* page = Div(a)->FlexCol()->Gap(12)->W(kFill);
    page->Child(StoryToolbar(cx, self));

    El* def = StorySection(cx, "Default",
                           "Text and icon toggles with clear selected states.");
    StorySectionBody(def)->FlexCol()->W(512)->ItemsCenter()->Gap(12);
    El* defRow = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    // Neither of the two says .outline(), so both are ghost.
    defRow->Child(ToggleChip(cx, onToggle, -1, "Preview", IconName::None,
                             self->toggleSel == 1, false));
    defRow->Child(ToggleChip(cx, onToggle, 0, nullptr, IconName::Star,
                             self->toggles[0], false));
    StorySectionAdd(def, defRow);
    page->Child(def);

    El* vars = StorySection(
        cx, "Variants", "Ghost and outline treatments for different surfaces.");
    StorySectionBody(vars)->W(512)->FlexCol()->ItemsCenter()->Gap(16);
    StorySectionAdd(vars, StoryTxt(cx, StrL("Ghost"), 14, th.foreground)
                              ->Medium());
    El* ghost = Div(a)->FlexRow()->Gap(4)->ItemsCenter();
    ghost->Child(ToggleChip(cx, onToggle, 1, nullptr, IconName::Bell,
                            self->toggles[1], false));
    ghost->Child(ToggleChip(cx, onToggle, 2, nullptr, IconName::Inbox,
                            self->toggles[2], false));
    ghost->Child(ToggleChip(cx, onToggle, 3, nullptr, IconName::Check,
                            self->toggles[3], false));
    StorySectionAdd(vars, ghost);
    StorySectionAdd(vars, StoryTxt(cx, StrL("Outline"), 14, th.foreground)
                              ->Medium());
    El* outline = Div(a)->FlexRow()->Gap(4)->ItemsCenter();
    outline->Child(ToggleChip(cx, onToggle, 4, nullptr, IconName::Bell,
                              self->toggles[4], true));
    outline->Child(ToggleChip(cx, onToggle, 5, nullptr, IconName::Inbox,
                              self->toggles[5], true));
    outline->Child(ToggleChip(cx, onToggle, 6, nullptr, IconName::Check,
                              self->toggles[6], true));
    StorySectionAdd(vars, outline);
    page->Child(vars);

    El* grp = StorySection(cx, "Group",
                           "Connected toggles keep related choices together.");
    StorySectionBody(grp)->W(512)->FlexCol()->ItemsCenter();
    // `.segmented().outline()`: the group has no box of its own — each
    // toggle carries the edges it needs.
    El* g =
        Div(a)->FlexRow()->ItemsCenter()->Radius(th.radius)->ClipX()->ClipY();
    g->Child(ToggleChip(cx, onToggle, 7, "Bold", IconName::None,
                        self->toggles[7], true, 0));
    g->Child(ToggleChip(cx, onToggle, 8, "Italic", IconName::None,
                        self->toggles[8], true, 1));
    g->Child(ToggleChip(cx, onToggle, 9, "Code", IconName::None,
                        self->toggles[9], true, 2));
    StorySectionAdd(grp, g);
    page->Child(grp);
    return page;
}

STORY_PAGE(StoryToggle, ToggleStory);
