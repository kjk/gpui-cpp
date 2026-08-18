#include "Story.h"

// The theme viewer groups every token the way the Rust story's mapper does;
// ours lists the tokens this port has.
struct ColorRow {
    const char* group;
    const char* name;
    Rgba color;
};

struct ThemeColorsStory {
    int openGroup = 0;
    bool showInherited = false;
    bool expandAll = false;
    bool optionsOpen = false;
    bool themeOpen = false;
    LineInput filter = {};
    bool seeded = false;

    static El* Render(ThemeColorsStory* self, Ctx* cx);
};

enum {
    ThemeActInherited = 400,
    ThemeActExpandAll
};

static void ThemeOptionsToggle(ThemeColorsStory* self, Ctx* cx,
                               const ClickEvent*) {
    self->optionsOpen = !self->optionsOpen;
    Notify(cx);
}
static void ThemeOptionAct(ThemeColorsStory* self, Ctx* cx, const ClickEvent*,
                           intptr_t act) {
    if (act == ThemeActInherited) {
        self->showInherited = !self->showInherited;
    } else {
        self->expandAll = !self->expandAll;
    }
    self->optionsOpen = false;
    Notify(cx);
}
static void ToggleColorGroup(ThemeColorsStory* self, Ctx* cx, const ClickEvent*,
                             intptr_t ix) {
    self->openGroup = self->openGroup == (int)ix ? -1 : (int)ix;
    Notify(cx);
}
static void ToggleThemeSelect(ThemeColorsStory* self, Ctx* cx,
                              const ClickEvent*) {
    self->themeOpen = !self->themeOpen;
    Notify(cx);
}
static void FocusFilter(ThemeColorsStory* self, Ctx* cx, const ClickEvent*) {
    self->filter.focused = true;
    Notify(cx);
}

static Str HexOf(Ctx* cx, Rgba c) {
    if (c.a == 255) {
        return StoryFmt(cx, "#%02x%02x%02x", c.r, c.g, c.b);
    }
    return StoryFmt(cx, "#%02x%02x%02x%02x", c.r, c.g, c.b, c.a);
}

El* ThemeColorsStory::Render(ThemeColorsStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    if (!self->seeded) {
        self->seeded = true;
        strncpy_s(self->filter.placeholder, "Search...", _TRUNCATE);
    }
    if (self->filter.focused) {
        cx->win->input = &self->filter;
    }

    const ColorRow rows[] = {
        {"Global", "Background", th.background},
        {"Global", "Border", th.border},
        {"Global", "Foreground", th.foreground},
        {"Global", "Ring", th.ring},
        {"Primary", "Background", th.primary},
        {"Primary", "Foreground", th.primaryFg},
        {"Secondary", "Background", th.secondary},
        {"Secondary", "Foreground", th.secondaryFg},
        {"Secondary", "Hover", th.secondaryHover},
        {"Secondary", "Active", th.secondaryActive},
        {"Accent", "Background", th.accent},
        {"Base", "Blue", th.blue},
        {"Base", "Cyan", th.cyan},
        {"Base", "Green", th.green},
        {"Base", "Magenta", th.magenta},
        {"Base", "Red", th.red},
        {"Base", "Yellow", th.yellow},
        {"Danger", "Background", th.danger},
        {"Danger", "Foreground", th.dangerFg},
        {"Info", "Background", th.info},
        {"Info", "Foreground", th.infoFg},
        {"Input", "Background", th.inputBg},
        {"Input", "Border", th.inputBorder},
        {"Muted", "Background", th.muted},
        {"Muted", "Foreground", th.mutedFg},
        {"Sidebar", "Background", th.sidebar},
        {"Sidebar", "Foreground", th.sidebarFg},
        {"Sidebar", "Primary", th.sidebarPrimary},
        {"Sidebar", "Primary Foreground", th.sidebarPrimaryFg},
        {"Success", "Background", th.success},
        {"Success", "Foreground", th.successFg},
        {"Warning", "Background", th.warning},
        {"Warning", "Foreground", th.warningFg},
    };
    const int nRows = (int)(sizeof(rows) / sizeof(rows[0]));

    El* page = Div(a)->FlexCol()->Gap(16)->W(kFill);

    // The theme picker, then the Options group at the far end.
    El* top = Div(a)
                  ->FlexRow()
                  ->W(kFill)
                  ->Gap(12)
                  ->ItemsCenter()
                  ->JustifyBetween()
                  ->FlexWrap();
    El* pick = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    pick->Child(component::Select::New(cx, StrL("theme-select"))
                    ->Option(StrL("Default Light"))
                    ->Option(StrL("Default Dark"))
                    ->Selected(0)
                    ->W(300)
                    ->Open(self->themeOpen)
                    ->OnToggle(Listen(cx, &ToggleThemeSelect))
                    ->IntoEl());
    pick->Child(component::Button::New(cx, StrL("set_theme"))
                    ->Label(StrL("Set Theme"))
                    ->Primary()
                    ->IntoEl());
    top->Child(pick);
    El* group = StoryToolbarGroup(cx);
    StoryToolbarOpt opts[2] = {
        {"Inherited Colors", self->showInherited, ThemeActInherited},
        {"Expand All", self->expandAll, ThemeActExpandAll},
    };
    group->Child(StoryToolbarDropdown(
        cx, StrL("theme-options"), StrL("Options"), self->optionsOpen,
        Listen(cx, &ThemeOptionsToggle), opts, 2, Listen(cx, &ThemeOptionAct)));
    top->Child(group);
    page->Child(top);

    El* body = Div(a)->FlexRow()->W(kFill)->Gap(16)->ItemsStart();

    // Left: the search field over the category list.
    El* left = Div(a)->FlexCol()->W(300)->Gap(8);
    left->Child(component::Input::New(cx, StrL("theme-filter"), &self->filter)
                    ->Prefix(Div(a)->PadL(10)->Child(
                        IconEl(a, IconName::Search, 16)->Fg(th.mutedFg)))
                    ->OnFocus(Listen(cx, &FocusFilter))
                    ->IntoEl());
    Listener toggleGroup = Listen(cx, &ToggleColorGroup);
    int groupIx = 0;
    for (int i = 0; i < nRows;) {
        const char* name = rows[i].group;
        int end = i;
        while (end < nRows && strcmp(rows[end].group, name) == 0) {
            end++;
        }
        bool open = self->expandAll || self->openGroup == groupIx;
        El* head = Div(a)
                       ->FlexRow()
                       ->W(kFill)
                       ->H(36)
                       ->PadX(8)
                       ->ItemsCenter()
                       ->JustifyBetween()
                       ->Radius(th.radius)
                       ->HoverBg(th.muted);
        head->Child(StoryTxt(cx, Str(name), 16, th.foreground));
        head->Child(
            IconEl(a, open ? IconName::ChevronDown : IconName::ChevronRight, 16)
                ->Fg(th.mutedFg));
        head->Click(HashClickId(StoryFmt(cx, "theme-group-%d", groupIx)))
            ->OnClick(ListenerArg(toggleGroup, groupIx));
        left->Child(head);
        if (open) {
            // The rows sit under a rail, indented from the group.
            El* items = Div(a)->FlexRow()->W(kFill)->PadL(12);
            items->Child(Div(a)->W(1)->H(kFill)->Bg(th.border));
            El* itemCol = Div(a)->FlexCol()->Grow();
            items->Child(itemCol);
            for (int r = i; r < end; r++) {
                El* row = Div(a)
                              ->FlexRow()
                              ->W(kFill)
                              ->H(32)
                              ->PadX(12)
                              ->ItemsCenter()
                              ->JustifyBetween();
                row->Child(StoryTxt(cx, Str(rows[r].name), 16, th.foreground));
                row->Child(Div(a)
                               ->W(16)
                               ->H(16)
                               ->Radius(3)
                               ->Bg(rows[r].color)
                               ->Border(1, th.border));
                itemCol->Child(row);
            }
            left->Child(items);
        }
        i = end;
        groupIx++;
    }
    body->Child(left);

    // Right: every group with its swatch, name and hex. Rust paints a
    // checkerboard behind the swatches to show alpha; ours is flat.
    El* right = Div(a)
                    ->FlexCol()
                    ->Grow()
                    ->H(640)
                    ->Pad(16)
                    ->Gap(8)
                    ->ClipY()
                    ->Radius(th.radiusLg)
                    ->Border(1, th.border)
                    ->Bg(th.muted);
    for (int i = 0; i < nRows;) {
        const char* name = rows[i].group;
        int end = i;
        while (end < nRows && strcmp(rows[end].group, name) == 0) {
            end++;
        }
        right->Child(
            StoryTxt(cx, Str(name), 16, th.foreground)->Medium()->PadY(8));
        for (int r = i; r < end; r++) {
            El* row = Div(a)->FlexRow()->W(kFill)->Gap(16)->ItemsCenter();
            row->Child(Div(a)
                           ->W(60)
                           ->H(60)
                           ->Radius(th.radius)
                           ->Bg(rows[r].color)
                           ->Border(1, th.border));
            El* text = Div(a)->FlexCol()->Gap(4);
            text->Child(StoryTxt(cx, Str(rows[r].name), 16, th.foreground)
                            ->Medium());
            text->Child(StoryTxt(cx, HexOf(cx, rows[r].color), 14, th.mutedFg));
            row->Child(text);
            right->Child(row);
        }
        i = end;
    }
    body->Child(right);
    page->Child(body);
    return page;
}

STORY_PAGE(StoryThemeColors, ThemeColorsStory);
