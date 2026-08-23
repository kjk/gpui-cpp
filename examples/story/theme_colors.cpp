#include "Story.h"

#include <math.h>

// The theme viewer groups every token the way the Rust story's mapper does;
// ours lists the tokens this port has.
struct ColorRow {
    const char* group;
    const char* name;
    // A fill, not a colour: the tokens schema.rs lets a theme spell as a
    // gradient are shown as one, so the swatch says what the token paints
    // rather than what its first stop is.
    Background color;
};

// The themes the picker offers: what the registry holds, which is the two
// out of default-theme.json plus every theme file it found. A SearchableList
// keeps a pointer to the items, so they outlive the frame — and the names
// point into the registry's own arena, which outlives everything.
static Vec<component::SearchableItem> gThemeItems;

// One entry per theme in the registry, in its sorted order, with the light
// ones in a section of their own so the list reads the way Rust's does.
static void FillThemeItems() {
    if (gThemeItems.len > 0) {
        return;
    }
    // `themes/`, wherever an asset root has one — the pinned Rust clone ships
    // twenty of them. Two entries are all the picker has without it.
    ThemeRegistryLoadDir(StrL("themes"));
    for (int i = 0; i < ThemeRegistryCount(); i++) {
        const ThemeConfig* cfg = ThemeRegistryAt(i);
        component::SearchableItem it = {};
        it.title = cfg->name;
        it.value = cfg->name;
        it.section = cfg->mode == ThemeMode::Dark ? 1 : 0;
        gThemeItems.Append(it);
    }
}

struct ThemeColorsStory {
    Entity<component::SearchableListState> themes = {};
    int openGroup = 0;
    bool showInherited = false;
    bool expandAll = false;
    bool optionsOpen = false;
    InputState filter;
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
    } else if (act == ThemeActExpandAll) {
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
    component::SelectToggleOpen(self->themes.Get(cx), cx);
}
// Theme::apply_config, from the row the picker has selected. Rust does it
// from the registry the same way; the palette the file resolves to replaces
// the one for its mode, and the window switches to that mode so the change is
// on screen rather than one menu away.
static void SetTheme(ThemeColorsStory* self, Ctx* cx, const ClickEvent*) {
    component::SearchableListState* st = self->themes.Get(cx);
    if (!st || st->selected.len == 0) {
        return;
    }
    int ix = st->selected[0];
    if (ix < 0 || ix >= gThemeItems.len) {
        return;
    }
    const ThemeConfig* cfg = ThemeRegistryFind(gThemeItems[ix].title);
    if (!cfg || !ThemeRegistryApply(cx->app, cfg)) {
        return;
    }
    ThemeSet(cx->app, cfg->mode);
    Notify(cx);
}

static void FocusFilter(ThemeColorsStory* self, Ctx* cx, const ClickEvent*) {
    self->filter.focused = true;
    Notify(cx);
}

// Checkerboard: the wash the theme viewer's swatches sit on, so a colour
// with alpha in it reads as translucent rather than as a darker opaque one.
// Rust paints it from a gpui::canvas over the panel background; the same two
// greys come out of customPaint here, which runs after the element's own fill
// and before its children.
static const float kCheckerSquare = 12.f;

static Rgba CheckerBase() {
    return ThemeGet() == ThemeMode::Dark ? RgbaHsla(0.f, 0.f, 0.1f, 1.f)
                                         : RgbaHsla(0.f, 0.f, 1.f, 1.f);
}

static void PaintCheckerboard(PaintCtx* ctx, El* e, void*) {
    Rgba c2 = ThemeGet() == ThemeMode::Dark ? RgbaHsla(0.f, 0.f, 0.13f, 1.f)
                                            : RgbaHsla(0.f, 0.f, 0.95f, 1.f);
    int rows = (int)ceilf(e->h / kCheckerSquare);
    int cols = (int)ceilf(e->w / kCheckerSquare);
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            if ((row + col) % 2 != 0) {
                continue;
            }
            CanvasFillRect(ctx, e->x + kCheckerSquare * (float)col,
                           e->y + kCheckerSquare * (float)row, kCheckerSquare,
                           kCheckerSquare, c2);
        }
    }
}

static Str HexOf(Ctx* cx, Rgba c) {
    if (c.a == 255) {
        return StoryFmt(cx, "#%02x%02x%02x", c.r, c.g, c.b);
    }
    return StoryFmt(cx, "#%02x%02x%02x%02x", c.r, c.g, c.b, c.a);
}

// The value a theme file would have to write to get this fill back.
static Str ValueOf(Ctx* cx, const Background& b) {
    if (!b.gradient) {
        return HexOf(cx, b.color);
    }
    return StoryFmt(cx, "linear-gradient(%gdeg, %s, %s)", (double)b.angle,
                    HexOf(cx, b.from.color), HexOf(cx, b.to.color));
}

El* ThemeColorsStory::Render(ThemeColorsStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    if (!self->seeded) {
        self->seeded = true;
        FillThemeItems();
        InputSetPlaceholder(&self->filter, StrL("Search..."));
        self->themes = EntityNewState<component::SearchableListState>(cx->app);
        component::SearchableListState* t = self->themes.Get(cx);
        if (t) {
            // The picker opens on the theme that is showing, which is the
            // one the registry has installed for the mode in force.
            Str active = ThemeRegistryActive(ThemeGet());
            int at = 0;
            for (int i = 0; i < gThemeItems.len; i++) {
                if (StrSame(gThemeItems[i].title, active)) {
                    at = i;
                    break;
                }
            }
            component::SearchableListSelectOnly(t, at);
        }
    }
    if (self->filter.focused) {
        cx->win->input = &self->filter;
    }

    // The names and the categories mapper.rs splits a theme key into,
    // for every token this port's Theme carries. Global, Primary and
    // Secondary lead; the rest run alphabetically, as they do in Rust.
    const ColorRow rows[] = {
        {"Global", "Background", th.tokens.background},
        {"Global", "Border", th.border},
        {"Global", "Foreground", th.foreground},
        {"Global", "Overlay", th.tokens.overlay},
        {"Global", "Ring", th.ring},
        {"Primary", "Background", th.tokens.primary},
        {"Primary", "Foreground", th.primaryFg},
        {"Secondary", "Active Background", th.secondaryActive},
        {"Secondary", "Background", th.tokens.secondary},
        {"Secondary", "Foreground", th.secondaryFg},
        {"Secondary", "Hover Background", th.secondaryHover},
        {"Accent", "Background", th.tokens.accent},
        {"Base", "Blue", th.blue},
        {"Base", "Cyan", th.cyan},
        {"Base", "Green", th.green},
        {"Base", "Magenta", th.magenta},
        {"Base", "Red", th.red},
        {"Base", "Yellow", th.yellow},
        {"Chart", "Bearish", th.chartBearish},
        {"Chart", "Bullish", th.chartBullish},
        {"Chart", "Color 1", th.chart1},
        {"Chart", "Color 2", th.chart2},
        {"Chart", "Color 3", th.chart3},
        {"Chart", "Color 4", th.chart4},
        {"Chart", "Color 5", th.chart5},
        {"Danger", "Background", th.tokens.danger},
        {"Danger", "Foreground", th.dangerFg},
        {"Description List", "Label Background", th.descListLabel},
        {"Description List", "Label Foreground", th.descListLabelFg},
        {"Group Box", "Background", th.groupBox},
        {"Group Box", "Foreground", th.groupBoxFg},
        {"Info", "Background", th.tokens.info},
        {"Info", "Foreground", th.infoFg},
        {"Input", "Background", th.inputBg},
        {"Input", "Border", th.inputBorder},
        {"Input", "Caret", th.caret},
        {"List", "Active Background", th.tokens.listActive},
        {"List", "Active Border", th.listActiveBorder},
        {"Muted", "Background", th.tokens.muted},
        {"Muted", "Foreground", th.mutedFg},
        {"Progress", "Background", th.tokens.progress},
        {"Scrollbar", "Thumb Background", th.tokens.scrollbarThumb},
        {"Sidebar", "Accent Background", th.tokens.sidebarAccent},
        {"Sidebar", "Accent Foreground", th.sidebarAccentFg},
        {"Sidebar", "Background", th.sidebar},
        {"Sidebar", "Border", th.sidebarBorder},
        {"Sidebar", "Foreground", th.sidebarFg},
        {"Sidebar", "Primary Background", th.tokens.sidebarPrimary},
        {"Sidebar", "Primary Foreground", th.sidebarPrimaryFg},
        {"Skeleton", "Background", th.tokens.skeleton},
        {"Slider", "Thumb Background", th.tokens.sliderThumb},
        {"Status Bar", "Background", th.tokens.statusBar},
        {"Success", "Background", th.tokens.success},
        {"Success", "Foreground", th.successFg},
        {"Switch", "Thumb Background", th.tokens.switchThumb},
        {"Tab", "Active Background", th.tokens.tabActiveBg},
        {"Tab", "Active Foreground", th.tabActiveFg},
        {"Tab", "Foreground", th.tabFg},
        {"Tab Bar", "Background", th.tokens.tabBar},
        {"Table", "Active Background", th.tokens.tableActive},
        {"Table", "Active Border", th.tableActiveBorder},
        {"Table", "Background", th.tokens.tableBg},
        {"Table", "Even Background", th.tokens.tableEven},
        {"Table", "Head Background", th.tokens.tableHead},
        {"Table", "Head Foreground", th.tableHeadFg},
        {"Table", "Row Border", th.tableRowBorder},
        {"Title Bar", "Background", th.tokens.titleBar},
        {"Title Bar", "Border", th.titleBarBorder},
        {"Warning", "Background", th.tokens.warning},
        {"Warning", "Foreground", th.warningFg},
    };
    const int nRows = (int)(sizeof(rows) / sizeof(rows[0]));

    El* page = Div(a)->FlexCol()->Gap(16)->W(kFill);

    // The theme picker on one row and the Options group on the next, the way
    // the Rust story stacks them.
    El* top = Div(a)->FlexCol()->W(kFill)->Gap(12);
    El* pick = Div(a)->FlexRow()->W(kFill)->Gap(8)->ItemsCenter();
    pick->Child(component::Select::New(cx, StrL("theme-select"), self->themes)
                    ->Items(gThemeItems.els, gThemeItems.len)
                    ->W(300)
                    ->OnToggle(Listen(cx, &ToggleThemeSelect))
                    ->IntoEl());
    pick->Child(component::Button::New(cx, StrL("set_theme"))
                    ->Label(StrL("Set Theme"))
                    ->Primary()
                    ->OnClick(Listen(cx, &SetTheme))
                    ->IntoEl());
    top->Child(pick);
    El* optRow = Div(a)->FlexRow()->W(kFill)->JustifyEnd();
    El* group = StoryToolbarGroup(cx);
    StoryToolbarOpt opts[2] = {
        {"Inherited Colors", self->showInherited, ThemeActInherited},
        {"Expand All", self->expandAll, ThemeActExpandAll},
    };
    group->Child(StoryToolbarDropdown(
        cx, StrL("theme-options"), StrL("Options"), self->optionsOpen,
        Listen(cx, &ThemeOptionsToggle), opts, 2, Listen(cx, &ThemeOptionAct)));
    optRow->Child(group);
    top->Child(optRow);
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
                       ->HoverBg(th.tokens.muted);
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
            El* itemCol = Div(a)->FlexCol()->Flex1();
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

    // Right: every group with its swatch, name and hex, over the
    // checkerboard that shows through a translucent colour.
    El* right = Div(a)
                    ->FlexCol()
                    ->Flex1()
                    ->H(640)
                    ->ClipY()
                    ->Radius(th.radiusLg)
                    ->Border(1, th.border)
                    ->Bg(CheckerBase());
    right->customPaint = PaintCheckerboard;
    El* inner = Div(a)->FlexCol()->W(kFill)->PadX(16);
    right->Child(inner);
    for (int i = 0; i < nRows;) {
        const char* name = rows[i].group;
        int end = i;
        while (end < nRows && strcmp(rows[end].group, name) == 0) {
            end++;
        }
        // v_flex().w_full().gap_3().pt_4()
        El* cat = Div(a)->FlexCol()->W(kFill)->Gap(12)->PadT(16);
        // text_base().font_semibold().pb_2().border_b_1()
        cat->Child(Div(a)
                       ->W(kFill)
                       ->PadB(8)
                       ->BorderB(1, th.border)
                       ->Child(StoryTxt(cx, Str(name), 16, th.foreground)
                                   ->Semibold()));
        // div().flex().flex_wrap().gap_4(), one w(px(220.)) cell per colour.
        El* wrap = Div(a)->FlexRow()->FlexWrap()->W(kFill)->Gap(16);
        for (int r = i; r < end; r++) {
            // h_flex().gap_3().items_center()
            El* row = Div(a)->FlexRow()->Gap(12)->ItemsCenter();
            // div().size_16().rounded(radius).border_1().flex_shrink_0()
            row->Child(Div(a)
                           ->W(64)
                           ->H(64)
                           ->Shrink0()
                           ->Radius(th.radius)
                           ->Bg(rows[r].color)
                           ->Border(1, th.border));
            El* text = Div(a)->FlexCol()->Gap(4)->Flex1();
            text->Child(StoryTxt(cx, Str(rows[r].name), 14, th.foreground)
                            ->Medium());
            // A gradient's value is long; the cell is 220 wide either way.
            text->Child(StoryTxt(cx, ValueOf(cx, rows[r].color), 14, th.mutedFg)
                            ->Truncate());
            row->Child(text);
            wrap->Child(Div(a)->W(220)->ClipX()->Child(row));
        }
        cat->Child(wrap);
        inner->Child(cat);
        i = end;
    }
    // pb_4 under the last category.
    inner->Child(Div(a)->H(16));
    body->Child(right);
    page->Child(body);
    return page;
}

STORY_PAGE(StoryThemeColors, ThemeColorsStory);
