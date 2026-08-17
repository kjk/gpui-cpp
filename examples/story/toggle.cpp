#include "Story.h"

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

static El* ToggleChip(Arena* a, int id, const char* label, IconName icon,
                      bool on, bool outline) {
    const Theme& th = ThemeNow();
    El* t = Toggle::New(a, StrDup(a, fmt("tog-%d", id)), id)
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
        t->Child(StoryTxt(a, Str(label), 13, th.foreground));
    }
    return t;
}

El* ToggleRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(a, app));

    El* def = StorySection(a, "Default",
                           "Text and icon toggles with clear selected states.");
    El* defRow = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    defRow->Child(ToggleChip(a, ClickTogglePreview, "Preview", IconName::None,
                             app->toggleSel == 1, true));
    defRow->Child(ToggleChip(a, ClickToggleStar, nullptr, IconName::Star,
                             app->toggles[0], true));
    StorySectionAdd(def, defRow);
    page->Child(def);

    El* vars = StorySection(
        a, "Variants", "Ghost and outline treatments for different surfaces.");
    El* varsCol = Div(a)->FlexCol()->Gap(16)->ItemsCenter();
    varsCol->Child(StoryTxt(a, StrL("Ghost"), 13, th.foreground)->Semibold());
    El* ghost = Div(a)->FlexRow()->Gap(4)->ItemsCenter();
    ghost->Child(ToggleChip(a, ClickToggleBell, nullptr, IconName::Bell,
                            app->toggles[1], false));
    ghost->Child(ToggleChip(a, ClickToggleInbox, nullptr, IconName::Inbox,
                            app->toggles[2], false));
    ghost->Child(ToggleChip(a, ClickToggleCheck, nullptr, IconName::Check,
                            app->toggles[3], false));
    varsCol->Child(ghost);
    varsCol->Child(StoryTxt(a, StrL("Outline"), 13, th.foreground)->Semibold());
    El* outline = Div(a)->FlexRow()->Gap(4)->ItemsCenter();
    outline->Child(ToggleChip(a, ClickToggleBell2, nullptr, IconName::Bell,
                              app->toggles[4], true));
    outline->Child(ToggleChip(a, ClickToggleInbox2, nullptr, IconName::Inbox,
                              app->toggles[5], true));
    outline->Child(ToggleChip(a, ClickToggleCheck2, nullptr, IconName::Check,
                              app->toggles[6], true));
    varsCol->Child(outline);
    StorySectionAdd(vars, varsCol);
    page->Child(vars);

    El* grp = StorySection(a, "Group",
                           "Connected toggles keep related choices together.");
    El* g = Div(a)->FlexRow()->Border(1, th.border)->Radius(th.radius);
    g->Child(ToggleChip(a, ClickToggleBold, "Bold", IconName::None,
                        app->toggles[7], false));
    g->Child(ToggleChip(a, ClickToggleItalic, "Italic", IconName::None,
                        app->toggles[8], false));
    g->Child(ToggleChip(a, ClickToggleCode, "Code", IconName::None,
                        app->toggles[9], false));
    StorySectionAdd(grp, g);
    page->Child(grp);
    return page;
}

void ToggleClick(StoryApp* app, int id) {
    if (id == ClickTogglePreview) {
        app->toggleSel = app->toggleSel == 1 ? 0 : 1;
        return;
    }
    int slot = id - ClickToggleStar;
    if (slot >= 0 && slot < 10) {
        app->toggles[slot] = !app->toggles[slot];
    }
}

STORY_PAGE(StoryToggle, ToggleRender, ToggleClick);
