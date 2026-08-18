#include "Story.h"

struct ToggleStory {
    int toggleSel = 1;
    bool toggles[10] = {};
    StoryToolbarState toolbar;

    static El* Render(ToggleStory* self, Ctx* cx);
    static void Click(ToggleStory* self, Ctx* cx, int id);
};

enum {
    ClickTogglePreview = 2300,
    ClickToggleStar,
    ClickToggleBell,
    ClickToggleInbox,
    ClickToggleCheck,
    ClickToggleBell2,
    ClickToggleInbox2,
    ClickToggleCheck2,
    ClickToggleBold,
    ClickToggleItalic,
    ClickToggleCode
};

static El* ToggleChip(Ctx* cx, int id, const char* label, IconName icon,
                      bool on, bool outline) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
    El* t = Toggle::New(cx, StrDup(a, fmt("tog-%d", id)), id)
                ->H(28)
                ->PadX(label ? 10.f : 8.f)
                ->ItemsCenter()
                ->JustifyCenter()
                ->Radius(th.radius)
                ->Gap(6);
    if (outline) {
        t->Border(1, th.border);
    }
    if (on) {
        t->Bg(th.accent);
    } else {
        t->HoverBg(th.muted);
    }
    if (icon != IconName::None) {
        t->Child(IconEl(a, icon, 14)->Fg(th.foreground));
    }
    if (label) {
        t->Child(StoryTxt(cx, Str(label), 13, th.foreground));
    }
    return t;
}

El* ToggleStory::Render(ToggleStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(cx, self));

    El* def = StorySection(cx, "Default",
                           "Text and icon toggles with clear selected states.");
    El* defRow = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    defRow->Child(ToggleChip(cx, ClickTogglePreview, "Preview", IconName::None,
                             self->toggleSel == 1, true));
    defRow->Child(ToggleChip(cx, ClickToggleStar, nullptr, IconName::Star,
                             self->toggles[0], true));
    StorySectionAdd(def, defRow);
    page->Child(def);

    El* vars = StorySection(
        cx, "Variants", "Ghost and outline treatments for different surfaces.");
    El* varsCol = Div(a)->FlexCol()->Gap(16)->ItemsCenter();
    varsCol->Child(StoryTxt(cx, StrL("Ghost"), 13, th.foreground)->Semibold());
    El* ghost = Div(a)->FlexRow()->Gap(4)->ItemsCenter();
    ghost->Child(ToggleChip(cx, ClickToggleBell, nullptr, IconName::Bell,
                            self->toggles[1], false));
    ghost->Child(ToggleChip(cx, ClickToggleInbox, nullptr, IconName::Inbox,
                            self->toggles[2], false));
    ghost->Child(ToggleChip(cx, ClickToggleCheck, nullptr, IconName::Check,
                            self->toggles[3], false));
    varsCol->Child(ghost);
    varsCol
        ->Child(StoryTxt(cx, StrL("Outline"), 13, th.foreground)->Semibold());
    El* outline = Div(a)->FlexRow()->Gap(4)->ItemsCenter();
    outline->Child(ToggleChip(cx, ClickToggleBell2, nullptr, IconName::Bell,
                              self->toggles[4], true));
    outline->Child(ToggleChip(cx, ClickToggleInbox2, nullptr, IconName::Inbox,
                              self->toggles[5], true));
    outline->Child(ToggleChip(cx, ClickToggleCheck2, nullptr, IconName::Check,
                              self->toggles[6], true));
    varsCol->Child(outline);
    StorySectionAdd(vars, varsCol);
    page->Child(vars);

    El* grp = StorySection(cx, "Group",
                           "Connected toggles keep related choices together.");
    El* g = Div(a)->FlexRow()->Border(1, th.border)->Radius(th.radius);
    g->Child(ToggleChip(cx, ClickToggleBold, "Bold", IconName::None,
                        self->toggles[7], false));
    g->Child(ToggleChip(cx, ClickToggleItalic, "Italic", IconName::None,
                        self->toggles[8], false));
    g->Child(ToggleChip(cx, ClickToggleCode, "Code", IconName::None,
                        self->toggles[9], false));
    StorySectionAdd(grp, g);
    page->Child(grp);
    return page;
}

void ToggleStory::Click(ToggleStory* self, Ctx* cx, int id) {
    (void)cx;
    if (id == ClickTogglePreview) {
        self->toggleSel = self->toggleSel == 1 ? 0 : 1;
        return;
    }
    int slot = id - ClickToggleStar;
    if (slot >= 0 && slot < 10) {
        self->toggles[slot] = !self->toggles[slot];
    }
}

STORY_PAGE(StoryToggle, ToggleStory);
