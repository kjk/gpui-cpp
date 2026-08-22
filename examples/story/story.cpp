#include "Story.h"
#include "gpui.h"

using namespace gpui;

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

static StoryPageNewFn gNew[StoryCount] = {};
static StoryPageKeyFn gKey[StoryCount] = {};

void StoryRegister(int story, StoryPageNewFn create, StoryPageKeyFn onKey) {
    if (story < 0 || story >= StoryCount) {
        return;
    }
    gNew[story] = create;
    gKey[story] = onKey;
}

// Resolve (creating on first view) the entity for the active story and render
// it with its own Ctx, so listeners inside a page bind to that page.
static EntityId StoryPageEntity(StoryApp* app, Ctx* cx) {
    int s = app->story;
    if (s < 0 || s >= StoryCount || !gNew[s]) {
        return EntityId{};
    }
    if (!app->pages[s].IsValid()) {
        app->pages[s] = gNew[s](cx->app);
    }
    return app->pages[s];
}

void StoryKeyRegistered(StoryApp* app, Ctx* cx, const KeyEvent* ev) {
    int s = app->story;
    if (s < 0 || s >= StoryCount || !gKey[s]) {
        return;
    }
    EntityId page = StoryPageEntity(app, cx);
    void* self = EntityGet(cx->app, page);
    if (!self) {
        return;
    }
    Ctx pageCx = *cx;
    pageCx.self = page;
    gKey[s](self, &pageCx, ev);
}

El* StoryRenderRegistered(StoryApp* app, Ctx* cx) {
    EntityId page = StoryPageEntity(app, cx);
    if (!page.IsValid()) {
        return StoryComingSoon(cx, app->story);
    }
    return EntityRender(cx->app, cx->win, cx->a, page);
}

static const StoryInfo kMeta[StoryCount] = {
    {"introduction", "Introduction",
     "UI components for building fantastic desktop application by using GPUI."},
    {"accordion", "Accordion",
     "The accordion uses collapse internally to make it collapsible."},
    {"alert", "Alert",
     "Communicate important status changes without interrupting the user's "
     "workflow."},
    {"alert-dialog", "AlertDialog",
     "Require a response before the user can continue."},
    {"avatar", "Avatar",
     "Represent a person or organization with an image or fallback."},
    {"badge", "Badge",
     "A red dot that indicates the number of unread messages."},
    {"breadcrumb", "Breadcrumb",
     "A breadcrumb navigation element that shows the current location in a "
     "hierarchy."},
    {"button", "Button",
     "Displays a button or a component that looks like a button."},
    {"calendar", "Calendar", "A calendar to select a date or date range."},
    {"chart", "Chart", "Beautiful Charts & Graphs."},
    {"checkbox", "Checkbox", "Select one or more independent options."},
    {"clipboard", "Clipboard",
     "Copy text or generated values to the clipboard."},
    {"collapsible", "Collapsible",
     "An interactive element that expands/collapses."},
    {"color-picker", "ColorPicker", "Choose and preview a color value."},
    {"combobox", "Combobox",
     "An autocomplete input paired with a searchable dropdown "
     "list."},
    {"data-table", "DataTable",
     "A complex data table with selection, sorting, column moving, "
     "and loading more."},
    {"date-picker", "DatePicker",
     "A date picker to select a date or date range."},
    {"description-list", "DescriptionList",
     "Present labels and values in a structured summary."},
    {"dialog", "Dialog", "Present focused content above the current view."},
    {"dock", "Dock",
     "A dockable layout of panels that can be moved, split and resized."},
    {"dropdown-button", "DropdownButton",
     "A button with an attached dropdown menu for additional "
     "options."},
    {"editor", "Editor",
     "Code editor with theme-aware syntax highlighting and "
     "folding."},
    {"form", "Form", "Form to collect multiple inputs."},
    {"group-box", "GroupBox",
     "A styled container element that with an optional title to groups "
     "related content together."},
    {"hover-card", "HoverCard",
     "A hover card displays content when hovering over a trigger "
     "element, with configurable delays."},
    {"icon", "Icon", "SVG Icons based on Lucide.dev"},
    {"image", "Image", "Image and SVG image supported."},
    {"input", "Input",
     "Capture and validate short-form text, credentials, "
     "identifiers, and formatted values."},
    {"kbd", "Kbd", "A tag style to display keyboard shortcuts"},
    {"label", "Label",
     "Display concise text with hierarchy, highlighting, and masking."},
    {"list", "List", "A list displays a series of items."},
    {"menu", "Menu", "Popup menu and context menu"},
    {"native-menu", "NativeMenu",
     "A menu rendered by the operating system. Unlike PopupMenu, "
     "it is drawn by the OS and can extend beyond the window "
     "bounds — useful for small windows."},
    {"notification", "Notification",
     "Show transient feedback without interrupting the current task."},
    {"number-input", "NumberInput",
     "Adjust constrained numeric values precisely with typing or "
     "increment and decrement controls."},
    {"otp-input", "OtpInput",
     "Enter short verification and recovery codes with clear "
     "grouping and masking controls."},
    {"pagination", "Pagination",
     "Pagination with page navigation, next and previous links."},
    {"popover", "Popover", "Show focused content beside a trigger."},
    {"progress", "Progress",
     "Show task completion with determinate or loading indicators."},
    {"radio", "Radio", "Choose one option from a set."},
    {"rating", "Rating", "A simple interactive star rating component."},
    {"resizable", "Resizable", "The resizable panels."},
    {"scrollbar", "Scrollbar", "Add scrollbar to a scrollable element."},
    {"searchable-list", "SearchableList",
     "The searchable, sectioned list behind a Select and a ComboBox."},
    {"select", "Select",
     "Displays a list of options for the user to pick "
     "from—triggered by a button."},
    {"separator", "Separator",
     "A separator that can be either vertical or horizontal."},
    {"settings", "Settings",
     "A collection of settings groups and items for the "
     "application."},
    {"sheet", "Sheet", "Sheet for open a popup in the edge of the window"},
    {"sidebar", "Sidebar",
     "A composable, themeable and customizable sidebar component."},
    {"skeleton", "Skeleton",
     "Use to show a placeholder while content is loading."},
    {"slider", "Slider",
     "Displays a slider control for selecting a value within a range."},
    {"spinner", "Spinner",
     "Displays an spinner showing the completion progress of a "
     "task."},
    {"status-bar", "StatusBar",
     "A horizontal bar with left/center/right regions, usually placed at the "
     "bottom."},
    {"stepper", "Stepper",
     "A step-by-step process for users to navigate through a series of "
     "steps."},
    {"switch", "Switch", "Turn a setting on or off."},
    {"table", "Table",
     "A basic table component for directly rendering tabular data."},
    {"tabs", "Tabs",
     "A set of layered sections of content—known as tab panels—that are "
     "displayed one at a time."},
    {"tag", "Tag",
     "A short item that can be used to categorize or label "
     "content."},
    {"textarea", "Textarea", "Input with multi-line mode."},
    {"theme-colors", "Theme Colors",
     "A color theme viewer to explore colors organized by "
     "categories."},
    {"tiles", "Tiles",
     "Panels that float over an area, each moved by its bar and resized by "
     "its edges."},
    {"toggle", "Toggle", "Turn an option on or off, alone or in a group."},
    {"tooltip", "Tooltip", "Describe a control on hover or keyboard focus."},
    // TreeStory has no description() in Rust, so its page has no line under
    // the title.
    {"tree", "Tree", nullptr},
    {"virtual-list", "VirtualList",
     "Add vertical or horizontal, or both scrollbars to a "
     "container, and use `virtual_list` to render a large number of "
     "items."},
};

const StoryInfo* StoryMeta(int i) {
    if (i < 0 || i >= StoryCount) {
        return &kMeta[0];
    }
    return &kMeta[i];
}

int StoryFromSlug(const char* slug) {
    if (!slug || !slug[0]) {
        return StoryWelcome;
    }
    for (int i = 0; i < StoryCount; i++) {
        if (StrEqI(Str(slug), Str(kMeta[i].slug)) ||
            StrEqI(Str(slug), Str(kMeta[i].title))) {
            return i;
        }
    }
    return StoryWelcome;
}

Str StoryDup(Ctx* cx, const char* s) {
    Arena* a = cx->a;
    return StrDup(a, Str(s));
}

const char* StoryArg(Ctx* cx, Str s) {
    // A Str need not be null-terminated, so printf gets a copy that is.
    const char* z = StrDup(cx->a, s).s;
    return z ? z : "";
}

Str StoryFmtV(Ctx* cx, const char* f, ...) {
    Arena* a = cx->a;
    char buf[512];
    va_list args;
    va_start(args, f);
    vsnprintf(buf, sizeof(buf), f, args);
    va_end(args);
    return StrDup(a, Str(buf));
}

El* StoryTxt(Ctx* cx, Str s, float px, Rgba c) {
    Arena* a = cx->a;
    return TextEl(a, s)->Font(px)->Fg(c);
}

El* StorySection(Ctx* cx, const char* title, const char* desc) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    // Rust StorySection is an outline GroupBox: title sits above a bordered
    // content pane that centers its children (crates/story/src/lib.rs).
    // mb_6 on the GroupBox: every section carries its own bottom margin, on
    // top of whatever gap the page sets.
    El* wrap = Div(a)->FlexCol()->Gap(12)->PadB(24)->W(kFill);
    // GroupBox draws its title with line_height(relative(1.)), which the
    // description inherits, so the header is 16 + 4 + 12 tall.
    // The header is a row: the title column, and whatever sub-title the page
    // adds opposite it.
    El* headRow =
        Div(a)->FlexRow()->W(kFill)->Gap(16)->ItemsStart()->JustifyBetween();
    El* head = Div(a)->FlexCol()->Gap(4);
    head->Child(StoryTxt(cx, StoryDup(cx, title), 16, th.mutedFg)
                    ->Medium()
                    ->LineHeight(1.f));
    if (desc && desc[0]) {
        head->Child(StoryTxt(cx, StoryDup(cx, desc), 12, th.mutedFg)
                        ->LineHeight(1.f)
                        ->Wrap());
    }
    // GroupBox's content pane, with StorySection's content_style on it:
    // bordered, p_4, rounded radius_lg, and centering the one child it has.
    El* pane = Div(a)
                   ->FlexCol()
                   ->Gap(16)
                   ->Pad(16)
                   ->W(kFill)
                   ->Border(1, th.border)
                   ->Radius(th.radiusLg)
                   ->ItemsCenter()
                   ->JustifyCenter();
    // section(): h_flex().w_full().flex_wrap().justify_center().items_center()
    // .gap_4() inside the pane. This is the element a page styles when it
    // says .w_128() or .v_flex() on its section.
    El* body = Div(a)
                   ->FlexRow()
                   ->FlexWrap()
                   ->Gap(16)
                   ->W(kFill)
                   ->ItemsCenter()
                   ->JustifyCenter();
    pane->Child(body);
    headRow->Child(head);
    wrap->Child(headRow);
    wrap->Child(pane);
    return wrap;
}

El* StorySectionSubTitle(El* section, El* sub) {
    if (!section || !sub || !section->first) {
        return section;
    }
    section->first->Child(sub);
    return section;
}

El* StorySectionBody(El* section) {
    if (!section || !section->first) {
        return nullptr;
    }
    // wrap = [headRow, pane]; pane = [body].
    El* pane = section->first;
    while (pane->next) {
        pane = pane->next;
    }
    return pane->first;
}

El* StorySectionAdd(El* section, El* child) {
    El* body = StorySectionBody(section);
    if (body && child) {
        body->Child(child);
    }
    return section;
}

static const char* StorySizeName(UiSize s) {
    switch (s) {
        case UiSize::XSmall:
            return "XSmall";
        case UiSize::Small:
            return "Small";
        case UiSize::Large:
            return "Large";
        default:
            return "Medium";
    }
}

// StoryToolbar::render joins its buttons into one segmented control: the
// group draws the outline, and the buttons after the first sit on their
// neighbour's border instead of drawing a second one.
static El* ToolbarGroup(Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    return Div(a)
        ->FlexRow()
        ->ItemsStart()
        ->Bg(th.tokens.background)
        ->Border(1, th.border)
        ->Radius(th.radius);
}

static El* ToolbarSep(Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    return Div(a)->W(1)->H(24)->Shrink0()->Bg(th.border);
}

// Button::outline().small(): h_6, px_2, text_sm. No Bg on the button — the
// group paints its background and border first, and an opaque child would
// cover the stroke that straddles the group's edge.
static El* ToolbarDropBtn(Ctx* cx, Str label) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    return Div(a)
        ->H(24)
        ->PadX(8)
        ->ItemsCenter()
        ->JustifyCenter()
        ->HoverBg(th.tokens.muted)
        ->Child(StoryTxt(cx, label, 14, th.foreground));
}

// PopupMenu::render_item: h 26, px_2, gap_x_1, text_sm, rounded, and a 12px
// icon gutter that Icon::empty() holds open on the rows that are not the
// checked one. `gutter` is Rust's has_left_icon: a menu with nothing checked
// has no column at all, so its rows sit flush left.
//
// Rust's rows fill the menu. A column here does not stretch its children, so
// they carry min_w(rems(8)) less the menu's padding instead, and the menu
// shrink-wraps around the widest of them.
static El* ToolbarCheckRow(Ctx* cx, Listener onAct, int act, const char* label,
                           bool on, bool gutter) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* row = Div(a)
                  ->H(26)
                  ->MinW(120)
                  ->PadX(8)
                  ->FlexRow()
                  ->Gap(4)
                  ->ItemsCenter()
                  ->Radius(th.radius)
                  ->HoverBg(th.tokens.accent);
    // The row needs a click id of its own, or HoverBg has nothing to match
    // against and the hovered row never lights up.
    row->Click(HashClickId(StoryFmt(cx, "story-toolbar-opt%d", act)))
        ->OnClick(ListenerArg(onAct, act));
    if (gutter) {
        El* mark = Div(a)->W(12)->H(12)->Shrink0();
        if (on) {
            mark->Child(IconEl(a, IconName::Check, 12)->Fg(th.foreground));
        }
        row->Child(mark);
    }
    row->Child(StoryTxt(cx, Str(label), 14, th.foreground));
    return row;
}

// popover_style, plus PopupMenu's p_1 and gap_y_0p5 around the items.
// PopupMenu::separator.
static El* ToolbarMenuSep(Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    // No width of its own: `align: stretch` is what makes it as wide as the
    // menu, where `W(kFill)` would make it as wide as whatever the menu is
    // floating over — and take the menu with it.
    return Div(a)->H(1)->Bg(th.border);
}

static El* ToolbarMenu(Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    return Div(a)
        ->FlexCol()
        ->Pad(4)
        ->Gap(2)
        ->Bg(th.tokens.background)
        ->Border(1, th.border)
        ->Radius(th.radius);
}

void StoryToolbarApply(StoryToolbarState* st, StoryAccordionOptions* opts,
                       int act) {
    switch (act) {
        case ToolbarOpenSize:
            st->sizeMenuOpen = !st->sizeMenuOpen;
            st->optsOpen = false;
            return;
        case ToolbarOpenOpts:
            st->optsOpen = !st->optsOpen;
            st->sizeMenuOpen = false;
            return;
        case ToolbarSizeXs:
            st->size = UiSize::XSmall;
            st->sizeMenuOpen = false;
            return;
        case ToolbarSizeSm:
            st->size = UiSize::Small;
            st->sizeMenuOpen = false;
            return;
        case ToolbarSizeMd:
            st->size = UiSize::Medium;
            st->sizeMenuOpen = false;
            return;
        case ToolbarSizeLg:
            st->size = UiSize::Large;
            st->sizeMenuOpen = false;
            return;
        default:
            break;
    }
    if (!opts) {
        return;
    }
    switch (act) {
        case ToolbarOptMultiple:
            opts->multiple = !opts->multiple;
            return;
        case ToolbarOptIcon:
            opts->icon = !opts->icon;
            return;
        case ToolbarOptDisabled:
            opts->disabled = !opts->disabled;
            return;
        case ToolbarOptBordered:
            opts->bordered = !opts->bordered;
            return;
        default:
            return;
    }
}

// The size dropdown, which most pages carry.
static El* StorySizeMenu(Ctx* cx, StoryToolbarState* st, Listener onAct) {
    El* sizeTrig =
        ToolbarDropBtn(cx, StoryFmt(cx, "Size: %s", StorySizeName(st->size)))
            ->OnClick(ListenerArg(onAct, ToolbarOpenSize));
    El* sizeMenu = nullptr;
    if (st->sizeMenuOpen) {
        // One size is always the current one, so the check column is always
        // there.
        sizeMenu = ToolbarMenu(cx);
        sizeMenu->Child(ToolbarCheckRow(cx, onAct, ToolbarSizeXs, "XSmall",
                                        st->size == UiSize::XSmall, true));
        sizeMenu->Child(ToolbarCheckRow(cx, onAct, ToolbarSizeSm, "Small",
                                        st->size == UiSize::Small, true));
        sizeMenu->Child(ToolbarCheckRow(cx, onAct, ToolbarSizeMd, "Medium",
                                        st->size == UiSize::Medium, true));
        sizeMenu->Child(ToolbarCheckRow(cx, onAct, ToolbarSizeLg, "Large",
                                        st->size == UiSize::Large, true));
    }
    return Popup::New(cx, StrL("story-size-menu"), sizeTrig)
        ->AnchorRight()
        ->Content(sizeMenu)
        ->IntoEl();
}

El* StoryToolbarCore(Ctx* cx, StoryToolbarState* st,
                     const StoryToolbarOpt* rows, int nrows, Listener onAct,
                     bool withSize) {
    Arena* a = cx->a;
    El* row = Div(a)->FlexRow()->W(kFill)->JustifyEnd()->ItemsStart();
    El* group = ToolbarGroup(cx);
    row->Child(group);

    if (withSize) {
        group->Child(StorySizeMenu(cx, st, onAct));
    }

    if (rows && nrows > 0) {
        group->Child(ToolbarSep(cx));
        El* optTrig = ToolbarDropBtn(cx, StrL("Options"))
                          ->OnClick(ListenerArg(onAct, ToolbarOpenOpts));
        El* optMenu = nullptr;
        if (st->optsOpen) {
            // has_left_icon: the column is there only while something in the
            // menu is checked. A row built with menu() rather than
            // menu_with_check() never is.
            bool gutter = false;
            for (int i = 0; i < nrows; i++) {
                gutter = gutter || (rows[i].checked && !rows[i].plain);
            }
            optMenu = ToolbarMenu(cx);
            for (int i = 0; i < nrows; i++) {
                if (rows[i].sep) {
                    optMenu->Child(ToolbarMenuSep(cx));
                }
                optMenu->Child(
                    ToolbarCheckRow(cx, onAct, rows[i].act, rows[i].label,
                                    rows[i].checked && !rows[i].plain, gutter));
            }
        }
        group->Child(Popup::New(cx, StrL("story-opts-menu"), optTrig)
                         ->AnchorRight()
                         ->Content(optMenu)
                         ->IntoEl());
    }
    return row;
}

El* StoryToolbarGroup(Ctx* cx) {
    return ToolbarGroup(cx);
}

El* StoryToolbarDivider(Ctx* cx) {
    return ToolbarSep(cx);
}

El* StoryToolbarDropdown(Ctx* cx, Str id, Str label, bool open, Listener onOpen,
                         const StoryToolbarOpt* rows, int nrows,
                         Listener onAct) {
    El* trigger = ToolbarDropBtn(cx, label)->OnClick(onOpen);
    El* menu = nullptr;
    if (open) {
        bool gutter = false;
        for (int i = 0; i < nrows; i++) {
            gutter = gutter || (rows[i].checked && !rows[i].plain);
        }
        menu = ToolbarMenu(cx);
        for (int i = 0; i < nrows; i++) {
            if (rows[i].sep) {
                menu->Child(ToolbarMenuSep(cx));
            }
            menu->Child(ToolbarCheckRow(cx, onAct, rows[i].act, rows[i].label,
                                        rows[i].checked && !rows[i].plain,
                                        gutter));
        }
    }
    return Popup::New(cx, id, trigger)->AnchorRight()->Content(menu)->IntoEl();
}

El* StoryComingSoon(Ctx* cx, int story) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    const StoryInfo* m = StoryMeta(story);
    return Div(a)
        ->FlexCol()
        ->Gap(8)
        ->Pad(8)
        ->Child(StoryTxt(cx, StoryDup(cx, m->title), 16, th.foreground)
                    ->Semibold())
        ->Child(StoryTxt(cx, StoryDup(cx, "This story is not ported yet."), 13,
                         th.mutedFg));
}

static bool StoryMatches(const StoryInfo* m, const char* q) {
    if (!q || !q[0]) {
        return true;
    }
    return StrContainsI(Str(m->title), Str(q)) ||
           StrContainsI(Str(m->slug), Str(q));
}

// Gallery::set_active_story
static void OpenStory(StoryApp* app, Ctx* cx, const ClickEvent*,
                      intptr_t story) {
    app->search.focused = false;
    cx->win->input = nullptr;
    app->story = (int)story;
    app->scrollY = 0;
    WindowSelectionClear(cx->win);
    Notify(cx);
}

static void FocusSearch(StoryApp* app, Ctx* cx, const ClickEvent*) {
    app->search.focused = true;
    cx->win->input = &app->search;
    Notify(cx);
}

static void ClearSearch(StoryApp* app, Ctx* cx, const ClickEvent*) {
    InputSetValue(&app->search, Str{});
    app->search.focused = false;
    cx->win->input = nullptr;
    Notify(cx);
}

// The pane that was scrolled reports where it should now be — by the wheel
// over it, or by a press or a drag on its bar. The view owns the offsets, so
// it is the one that stores them.
static void OnPaneScroll(StoryApp* app, Ctx* cx, const ScrollEvent* ev) {
    if (ev->id == 2) {
        app->sideScrollY = ev->offsetY;
    } else {
        app->scrollY = ev->offsetY;
    }
    Notify(cx);
}

static El* SidebarList(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* list = Div(a)->FlexCol()->Gap(2)->Pad(8);
    const char* q = InputCStr(&app->search);
    for (int i = 0; i < StoryCount; i++) {
        const StoryInfo* m = StoryMeta(i);
        if (!StoryMatches(m, q)) {
            continue;
        }
        bool on = app->story == i;
        El* row = Div(a)
                      ->H(32)
                      ->W(kFill)
                      ->PadX(10)
                      ->ItemsCenter()
                      ->Radius(6)
                      ->OnClick(Listen(cx, &OpenStory, i))
                      ->FocusId(ClickStory + i);
        // SidebarMenuItem is text_sm.
        El* label = StoryTxt(cx, Str(m->title), 14, th.sidebarFg);
        if (on) {
            label->Semibold();
            row->Bg(th.tokens.secondary);
        } else {
            row->HoverBg(th.tokens.secondary);
        }
        row->Child(label);
        list->Child(row);
    }
    return list;
}

static El* SearchBox(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* box = Div(a)
                  ->H(36)
                  ->W(kFill)
                  ->PadX(14)
                  ->FlexRow()
                  ->ItemsCenter()
                  ->JustifyBetween()
                  ->Gap(8)
                  ->Radius(18)
                  ->Bg(th.tokens.secondary)
                  ->OnClick(Listen(cx, &FocusSearch))
                  ->FocusId(ClickSearch);
    box->Child(::Input::New(cx, &app->search)->Flex1());
    if (InputValue(&app->search).len > 0) {
        box->Child(Div(a)
                       ->W(16)
                       ->H(16)
                       ->ItemsCenter()
                       ->JustifyCenter()
                       ->Shrink0()
                       ->OnClick(Listen(cx, &ClearSearch))
                       ->Child(IconEl(a, IconName::X, 12)->Fg(th.mutedFg)));
    }
    return box;
}

// gallery.rs wraps the sidebar in
//   resizable_panel().size(px(255.)).size_range(px(200.)..px(320.))
// GPUI turns that 255 into a fraction of the group at first layout, and the
// story window opens at 1600 wide (crates/story/src/lib.rs), so the sidebar
// tracks 255/1600 of the window width, clamped to the size_range. It is 200 at
// half a 1920 screen and 221 at 1400, which is what the Rust app draws.
static float SidebarWidth(Ctx* cx) {
    float w = WindowSize(cx->win).dipW * (255.f / 1600.f);
    w = (float)(int)(w + 0.5f); // GPUI rounds; truncating is off by one at 1400
    if (w < 200.f) {
        w = 200.f;
    }
    if (w > 320.f) {
        w = 320.f;
    }
    return w;
}

static El* Sidebar(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    float w = app->collapsed ? 56.f : SidebarWidth(cx);
    // Rust puts this sidebar in a `resizable_panel()`, whose width is not up
    // for negotiation with the pane beside it. A plain flex item is, so it
    // says so: without this the content pane's own minimum squeezes the
    // sidebar below the 200 the width helper floors at.
    El* side = Div(a)->W(w)->H(kFill)->FlexCol()->Shrink0()->Bg(th.sidebar);
    El* header = Div(a)->FlexCol()->Pad(12)->Gap(16);
    El* brand = Div(a)->FlexRow()->Gap(10)->ItemsCenter();
    El* logo = Div(a)
                   ->W(32)
                   ->H(32)
                   ->Radius(8)
                   ->Bg(th.tokens.primary)
                   ->ItemsCenter()
                   ->JustifyCenter()
                   ->Shrink0()
                   ->Child(IconEl(a, IconName::GalleryVerticalEnd, 16)
                               ->Fg(th.primaryFg));
    brand->Child(logo);
    if (!app->collapsed) {
        El* names = Div(a)->FlexCol();
        names->Child(StoryTxt(cx, StrL("GPUI Component"), 14, th.sidebarFg)
                         ->Semibold());
        names->Child(StoryTxt(cx, StrL("Component showcase"), 12, th.mutedFg));
        brand->Child(names);
    }
    header->Child(brand);
    if (!app->collapsed) {
        header->Child(SearchBox(app, cx));
    }
    side->Child(header);
    El* scroller = Div(a)
                       ->FlexCol()
                       ->Flex1()
                       ->MinH(0)
                       ->ClipY()
                       ->ScrollY(app->sideScrollY)
                       ->ScrollId(2)
                       ->OnScroll(Listen(cx, &OnPaneScroll))
                       ->W(kFill);
    if (!app->collapsed) {
        scroller->Child(SidebarList(app, cx));
    }
    side->Child(scroller);
    return side;
}

static El* Header(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    const StoryInfo* m = StoryMeta(app->story);
    return Div(a)
        ->W(kFill)
        ->Pad(16)
        ->FlexCol()
        ->Gap(4)
        ->Shrink0()
        ->BorderB(1, th.border)
        ->Child(StoryTxt(cx, Str(m->title), 24, th.foreground)->Semibold())
        ->Child(StoryTxt(cx, Str(m->description), 16, th.mutedFg)->Wrap());
}

// AppTitleBar in crates/story, on top of component::TitleBar: the menu and
// tool buttons claim their own hit rectangles, and the surface left over is
// the window drag region.
static El* StoryTitleMenuItem(Ctx* cx, const char* label, bool semibold) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* text = StoryTxt(cx, Str(label), 14, th.foreground);
    if (semibold) {
        text->Semibold();
    }
    return Div(a)
        ->H(kFill)
        ->PadX(8)
        ->ItemsCenter()
        ->Radius(th.radius)
        ->Click(HashClickId(StoryFmt(cx, "story-title-%s", label)))
        ->HoverBg(th.tokens.muted)
        ->Child(text);
}

// The window's notification list, which is now the runtime's:
// `window.notifications(cx)` and `window.push_notification(..)` are
// WindowExt, so a page pushes one without the app entity being in the way and
// a notification outlives leaving the page that raised it.
Entity<component::NotificationListState> StoryNotifications(Ctx* cx) {
    return WindowNotifications(cx);
}

void StoryPushNotification(Ctx* cx, Str message) {
    WindowPushNotification(cx, component::NotificationKind::Info, message);
}

static int StoryNotificationCount(Ctx* cx) {
    return WindowNotificationCount(cx);
}

// AppTitleBar's FontSizeSelector, which is the Appearance menu behind the
// Settings2 button. Rust gives each row an action — SelectFont, SelectRadius,
// SelectScrollbarMode, ToggleListActiveHighlight, ToggleFpsMonitor — and
// dispatches it through the focus chain; a PopupMenu here reports which row
// was taken, so one table both builds the menu and says what each row does.
enum class ApKind : uint8_t {
    Label,
    Sep,
    Font,
    Radius,
    Scroll,
    ListHighlight,
    Fps,
    Reduce,
    Ring
};

struct ApRow {
    ApKind kind;
    const char* label;
    // The font size or radius in DIPs, or the scrollbar mode; unused by the
    // two toggles, which read what they toggle.
    float value;
};

static const ApRow kAppearance[] = {
    {ApKind::Label, "Font Size", 0},
    {ApKind::Font, "Large", 18},
    {ApKind::Font, "Medium (default)", 16},
    {ApKind::Font, "Small", 14},
    {ApKind::Sep, nullptr, 0},
    {ApKind::Label, "Border Radius", 0},
    {ApKind::Radius, "8px", 8},
    {ApKind::Radius, "6px (default)", 6},
    {ApKind::Radius, "4px", 4},
    {ApKind::Radius, "0px", 0},
    {ApKind::Sep, nullptr, 0},
    {ApKind::Label, "Scrollbar", 0},
    {ApKind::Scroll, "Scrolling", (float)ScrollbarMode::Scrolling},
    {ApKind::Scroll, "Hover to show", (float)ScrollbarMode::Hover},
    {ApKind::Scroll, "Always show", (float)ScrollbarMode::Always},
    {ApKind::Sep, nullptr, 0},
    {ApKind::ListHighlight, "List Active Highlight", 0},
    {ApKind::Fps, "FPS Monitor", 0},
    // Not a row Rust's menu has. `cx.reduce_motion()` is the desktop's own
    // setting, which a gallery of components that move is the one place you
    // would want to try both ways without leaving to change it.
    {ApKind::Reduce, "Reduce Motion", 0},
    // Nor this one. `Theme::focus_ring` is meant to be set once by an
    // application whose layout clips its containers; a gallery is where you
    // can see what the two look like side by side.
    {ApKind::Ring, "Focus Ring", 0},
};

static const int kAppearanceRows = (int)(sizeof(kAppearance) / sizeof(ApRow));

// menu_with_check: which row is the one in force.
static bool ApChecked(const StoryApp* app, const ApRow& r) {
    switch (r.kind) {
        case ApKind::Font:
            return ThemeFontSize() == r.value;
        case ApKind::Radius:
            return ThemeNow().radius == r.value;
        case ApKind::Scroll:
            return ScrollbarModeNow() == (ScrollbarMode)(int)r.value;
        case ApKind::ListHighlight:
            return ListSettingsNow().activeHighlight;
        case ApKind::Fps:
            return app->fpsMonitor;
        case ApKind::Reduce:
            return MotionReduced();
        case ApKind::Ring:
            return ThemeFocusRing();
        default:
            return false;
    }
}

static void OnAppearanceItem(StoryApp* app, Ctx* cx, const ClickEvent*,
                             intptr_t ix) {
    if (ix < 0 || ix >= kAppearanceRows) {
        return;
    }
    const ApRow& r = kAppearance[ix];
    switch (r.kind) {
        case ApKind::Font:
            ThemeSetFontSize(r.value);
            break;
        case ApKind::Radius:
            ThemeSetRadius(r.value);
            break;
        case ApKind::Scroll:
            ScrollbarModeSet((ScrollbarMode)(int)r.value);
            break;
        case ApKind::ListHighlight: {
            ListSettings s = ListSettingsNow();
            s.activeHighlight = !s.activeHighlight;
            ListSettingsSet(s);
            break;
        }
        case ApKind::Fps:
            app->fpsMonitor = !app->fpsMonitor;
            break;
        case ApKind::Reduce:
            MotionSetReduced(!MotionReduced());
            break;
        case ApKind::Ring:
            ThemeSetFocusRing(!ThemeFocusRing());
            break;
        default:
            break;
    }
    // window.refresh(), and the layout memo goes with it: a font size or a
    // radius changes every box that inherited one.
    Notify(cx);
}

// create_new_window_with_size: everything one gallery window is, so the menu
// item below and GpuiMain open the same thing. Rust passes 1600x1200 and lets
// GPUI cap it at 85% of the display and centre it, which is what WindowOpen
// does here.
static void OnUnhandledClick(StoryApp* app, Ctx* cx, const ClickEvent* ev);
static void OnKey(StoryApp* app, Ctx* cx, const KeyEvent* ev);

static Window* StoryOpenWindow(App* app, int story) {
    Entity<StoryApp> view = EntityNew<StoryApp>(app);
    StoryApp* self = view.Get(app);
    if (!self) {
        return nullptr;
    }
    self->story = story;
    InputSetPlaceholder(&self->search, StrL("Search…"));
    WinOpts opts = {};
    // TitleBar::window_options(): the story owns its title bar on every
    // platform. macOS keeps the traffic lights over a transparent one,
    // Windows and X11 have none, so component::TitleBar draws the minimize /
    // maximize / close controls there itself.
    opts.clientTitleBar = true;
    Window* win = WindowOpenView(app, StrL("GPUI Component C++"), 1600, 1200,
                                 view.id, opts);
    if (!win) {
        return nullptr;
    }
    WindowOnUnhandledClick(win, ListenTo(view, &OnUnhandledClick));
    WindowOnKey(win, ListenTo(view, &OnKey));
    return win;
}

// The Window menu. Rust's has one item, Toggle Search; these two are what a
// second window needs to be reachable at all — `App` has held a window list
// and ended its loop with the last one for a while, and nothing opened one.
static void OnWindowMenuItem(StoryApp*, Ctx* cx, const ClickEvent*,
                             intptr_t ix) {
    if (ix == 0) {
        StoryOpenWindow(cx->app, StoryFromSlug(""));
    } else if (ix == 1) {
        AppClose(cx->win);
    }
}

// The About dialog, which the Help menu raises. It is an entity of its own
// rather than something a page renders, which is what WindowExt is for: the
// menu handler has no view that draws dialogs and does not need one, and the
// dialog outlives whichever page happens to be showing. Rust writes the same
// thing as `window.open_alert_dialog(cx, |alert, ..| ..)`.
struct AboutDialog {
    static void OnClose(AboutDialog*, Ctx* cx, const ClickEvent*) {
        WindowCloseDialog(cx);
    }

    static El* Render(AboutDialog*, Ctx* cx) {
        Arena* a = cx->a;
        const Theme& th = cx->theme();
        El* body = Div(a)->FlexCol()->Gap(8)->W(kFill);
        body->Child(
            StoryTxt(cx,
                     StrL("A C++ port of longbridge/gpui-component: the "
                          "same components, the same theme, no Rust and "
                          "no STL."),
                     14, th.mutedFg)
                ->W(kFill)
                ->Wrap());
        body->Child(StoryTxt(cx, StrL("github.com/longbridge/gpui-component"),
                             14, th.mutedFg)
                        ->W(kFill));
        Listener close = Listen(cx, &AboutDialog::OnClose);
        return component::Dialog::New(cx)
            ->Open(true)
            ->Title(StrL("GPUI Component"))
            ->Description(StrL("Component showcase  v0.5.1"))
            ->Body(body)
            ->W(420)
            ->CloseButton()
            ->OkText(StrL("Close"))
            ->OnOk(close)
            ->OnClose(close)
            ->OnCancel(close)
            ->IntoEl(WindowSize(cx->win));
    }
};

static void OnHelpMenuItem(StoryApp*, Ctx* cx, const ClickEvent*, intptr_t ix) {
    if (ix == 0) {
        OpenUrl(StrL("https://github.com/longbridge/gpui-component"));
    } else if (ix == 1) {
        // window.open_dialog(cx, ..): the window takes the entity and Root
        // draws it, whatever page is up.
        WindowOpenDialog(cx, EntityNew<AboutDialog>(cx->app));
    }
}

static El* HelpMenu(Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    component::PopupMenu* menu =
        component::PopupMenu::New(cx, StrL("story-help-menu"));
    menu->Menu(StrL("Documentation"));
    menu->Menu(StrL("About GPUI Component"));
    if (PopupMenuState* st = menu->state.Get(cx)) {
        st->onConfirm = Listen(cx, &OnHelpMenuItem);
    }
    return component::DropdownMenu::New(cx, StrL("story-help"))
        ->Trigger(Div(a)
                      ->H(28)
                      ->PadX(8)
                      ->ItemsCenter()
                      ->Radius(th.radius)
                      ->HoverBg(th.tokens.muted)
                      ->Cursor(CursorKind::Pointer)
                      ->Child(StoryTxt(cx, StrL("Help"), 14, th.foreground)))
        ->Menu(menu)
        ->IntoEl();
}

// app_menus.rs's Edit menu. Every row names one of the input's actions and
// carries no handler of its own: choosing it dispatches the action to
// whatever field has the keyboard, which is the same handler the chord
// reaches, and the shortcut on the right is looked up in the keymap rather
// than typed here.
static El* EditMenu(Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    component::PopupMenu* menu =
        component::PopupMenu::New(cx, StrL("story-edit-menu"))
            ->MinW(220)
            // The field's own key context, which is where those actions are
            // bound and so where their chords are found.
            ->ActionContext("Input")
            ->MenuWithAction(StrL("Undo"), input::Undo())
            ->MenuWithAction(StrL("Redo"), input::Redo())
            ->Separator()
            ->MenuWithAction(StrL("Cut"), input::Cut())
            ->MenuWithAction(StrL("Copy"), input::Copy())
            ->MenuWithAction(StrL("Paste"), input::Paste())
            ->Separator()
            ->MenuWithAction(StrL("Delete"), input::Delete())
            ->MenuWithAction(StrL("Delete Previous Word"),
                             input::DeleteToPreviousWordStart())
            ->MenuWithAction(StrL("Delete Next Word"),
                             input::DeleteToNextWordEnd())
            ->Separator()
            ->MenuWithAction(StrL("Find"), input::Search())
            ->Separator()
            ->MenuWithAction(StrL("Select All"), input::SelectAll());
    return component::DropdownMenu::New(cx, StrL("story-edit"))
        ->Trigger(Div(a)
                      ->H(28)
                      ->PadX(8)
                      ->ItemsCenter()
                      ->Radius(th.radius)
                      ->HoverBg(th.tokens.muted)
                      ->Cursor(CursorKind::Pointer)
                      ->Child(StoryTxt(cx, StrL("Edit"), 14, th.foreground)))
        ->Menu(menu)
        ->IntoEl();
}

static El* WindowMenu(Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    component::PopupMenu* menu =
        component::PopupMenu::New(cx, StrL("story-window-menu"));
    menu->Menu(StrL("New Window"));
    menu->Menu(StrL("Close Window"));
    if (PopupMenuState* st = menu->state.Get(cx)) {
        st->onConfirm = Listen(cx, &OnWindowMenuItem);
    }
    return component::DropdownMenu::New(cx, StrL("story-window"))
        ->Trigger(Div(a)
                      ->H(28)
                      ->PadX(8)
                      ->ItemsCenter()
                      ->Radius(th.radius)
                      ->HoverBg(th.tokens.muted)
                      ->Cursor(CursorKind::Pointer)
                      ->Child(StoryTxt(cx, StrL("Window"), 14, th.foreground)))
        ->Menu(menu)
        ->IntoEl();
}

static El* AppearanceMenu(StoryApp* app, Ctx* cx) {
    component::PopupMenu* menu =
        component::PopupMenu::New(cx, StrL("story-appearance-menu"));
    for (int i = 0; i < kAppearanceRows; i++) {
        const ApRow& r = kAppearance[i];
        switch (r.kind) {
            case ApKind::Label:
                menu->Label(Str(r.label));
                break;
            case ApKind::Sep:
                menu->Separator();
                break;
            default:
                menu->MenuWithCheck(Str(r.label), ApChecked(app, r));
                break;
        }
    }
    // check_side(Right): the tick sits on the far edge, so the labels start
    // flush.
    menu->CheckSide(Side::Right);
    if (PopupMenuState* st = menu->state.Get(cx)) {
        st->onConfirm = Listen(cx, &OnAppearanceItem);
    }
    return component::DropdownMenu::New(cx, StrL("story-appearance"))
        ->Trigger(component::Button::New(cx, StrL("story-title-settings"))
                      ->Icon(IconName::Settings2)
                      ->Ghost()
                      ->Compact()
                      ->WithSize(UiSize::Small)
                      ->Tooltip(StrL("Appearance"))
                      ->IntoEl()
                      ->Cursor(CursorKind::Pointer))
        ->Menu(menu)
        // Anchor::TopRight: the menu's right edge lines up with the button's,
        // which is what keeps it on screen at the corner of the window.
        ->AnchorRight()
        ->IntoEl();
}

static void OnGithub(StoryApp*, Ctx*, const ClickEvent*) {
    OpenUrl(StrL("https://github.com/longbridge/gpui-component"));
}

// The three tools at the right of the title bar. They are ghost buttons, and
// button.rs gives a ghost button the arrow — the hand is for the variants that
// look like a link. Over a title bar there is nothing else to say an icon is a
// control rather than an ornament, so these three ask for it themselves.
static El* StoryTitleBar(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;

    El* menus = Div(a)
                    ->FlexRow()
                    ->H(kFill)
                    ->ItemsCenter()
                    ->Child(StoryTitleMenuItem(cx, "GPUI Component", true))
                    ->Child(EditMenu(cx))
                    ->Child(WindowMenu(cx))
                    ->Child(HelpMenu(cx));
    El* tools =
        Div(a)
            ->FlexRow()
            ->H(kFill)
            ->ItemsCenter()
            ->PadX(8)
            ->Gap(2)
            ->Child(AppearanceMenu(app, cx))
            ->Child(component::Button::New(cx, StrL("story-title-github"))
                        ->Icon(IconName::Github)
                        ->Ghost()
                        ->Compact()
                        ->WithSize(UiSize::Small)
                        ->Tooltip(StrL("GitHub"))
                        ->OnClick(Listen(cx, &OnGithub))
                        ->IntoEl()
                        ->Cursor(CursorKind::Pointer))
            // Badge::count: how many notifications are up, capped at 99. The
            // bell itself has nothing to do in Rust either — the count is the
            // whole of it.
            ->Child(
                component::Badge::New(cx)
                    ->Count(StoryNotificationCount(cx))
                    ->Max(99)
                    ->Child(component::Button::New(cx, StrL("story-title-bell"))
                                ->Icon(IconName::Bell)
                                ->Ghost()
                                ->Compact()
                                ->WithSize(UiSize::Small)
                                ->Tooltip(StrL("Notifications"))
                                ->IntoEl()
                                ->Cursor(CursorKind::Pointer))
                    ->IntoEl());

    return component::TitleBar::New(cx)->Child(menus)->Child(tools)->IntoEl();
}

static El* Footer(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    const StoryInfo* m = StoryMeta(app->story);
    return Div(a)
        ->W(kFill)
        ->H(28)
        ->PadX(12)
        ->ItemsCenter()
        ->JustifyBetween()
        ->Shrink0()
        ->Bg(th.tokens.titleBar)
        ->BorderT(1, th.border)
        ->Child(
            Div(a)
                ->FlexRow()
                ->Gap(8)
                ->ItemsCenter()
                ->Child(IconEl(a, IconName::GalleryVerticalEnd, 12)
                            ->Fg(th.mutedFg))
                ->Child(StoryTxt(cx, StoryFmt(cx, "%d components", StoryCount),
                                 12, th.mutedFg))
                ->Child(Div(a)->W(1)->H(12)->Bg(th.border))
                ->Child(StoryTxt(cx, Str(m->title), 12, th.mutedFg)))
        ->Child(Div(a)
                    ->FlexRow()
                    ->Gap(12)
                    ->ItemsCenter()
                    // The theme in force, which is whatever the registry
                    // last installed for this mode rather than always one of
                    // the two defaults.
                    ->Child(StoryTxt(cx, ThemeRegistryActive(ThemeGet()), 12,
                                     th.mutedFg))
                    ->Child(StoryTxt(cx, StrL("v0.5.1"), 12, th.mutedFg)));
}

El* StoryApp::Render(StoryApp* app, Ctx* cx) {
    Arena* frame = cx->a;
    // Pages that own a text field point the window at it from their Render.
    if (app->search.focused) {
        cx->win->input = &app->search;
    }
    const Theme& th = cx->theme();
    if (!app->seeded) {
        app->seeded = true;
    }
    // The window's outermost view is a Root, which is what Rust puts under
    // every window: the page, and over it the layers the window owns.
    El* root = Div(frame)->FlexCol()->SizeFull();
    if (cx->win->opts.clientTitleBar) {
        root->Child(StoryTitleBar(app, cx));
    }
    El* body = Div(frame)->FlexRow()->Flex1()->W(kFill)->MinH(0)->H(kFill);
    body->Child(Sidebar(app, cx));
    // The resizable handle reads as a 1px rule. Rust anchors it over the
    // boundary rather than in the flow, so the content starts where the
    // sidebar ends; a border here is painted inside the box without taking
    // space from it, which puts the rule in the same place.
    El* main = Div(frame)->FlexCol()->Flex1()->H(kFill)->MinW(0)->BorderL(
        1, th.border);
    main->Child(Header(app, cx));
    El* scroller = Div(frame)
                       ->FlexCol()
                       ->Flex1()
                       ->MinH(0)
                       ->ClipY()
                       ->ScrollY(app->scrollY)
                       ->ScrollId(1)
                       ->OnScroll(Listen(cx, &OnPaneScroll))
                       ->W(kFill);
    scroller->Child(
        Div(frame)->Pad(16)->W(kFill)->Child(StoryRenderRegistered(app, cx)));
    main->Child(scroller);
    body->Child(main);
    // The inspector docks on the right, as it does off Root in Rust.
    // Ctrl+Shift+I toggles it.
    if (El* inspector = component::Inspector::New(cx)->IntoEl()) {
        body->Child(inspector);
    }
    root->Child(body);
    root->Child(Footer(app, cx));
    // ToggleFpsMonitor: the HUD places itself over the top right corner.
    if (app->fpsMonitor) {
        root->Child(FpsMonitorEl(cx));
    }
    // Bordered only where the window is client-decorated; a system frame
    // draws its own, and Rust's window_border is the Linux CSD wrapper.
    return component::Root::New(cx)
        ->Bordered(cx->win->opts.clientTitleBar)
        ->Child(root)
        ->IntoEl();
}

static void OnUnhandledClick(StoryApp* app, Ctx* cx, const ClickEvent* ev) {
    // A click that no element claimed lands here: dismiss the search field
    // the way GPUI dismisses an overlay on an outside click.
    if (ev->id == 0 && app->search.focused) {
        app->search.focused = false;
        cx->win->input = nullptr;
    }
}

static void OnChar(StoryApp* app, Ctx* cx, const KeyEvent* ev) {
    (void)cx;
    uint32_t cp = ev->ch;
    if (app->search.focused) {
        (void)cp;
    }
}

static void OnKey(StoryApp* app, Ctx* cx, const KeyEvent* ev) {
    int vk = ev->vk;
    bool down = ev->down;
    if (ev->ch != 0) {
        OnChar(app, cx, ev);
        return;
    }
    if (!down) {
        return;
    }
    // Ctrl+C is the window's now — WindowKeyDown copies the selection before
    // this handler runs — so the shell only has Escape left to answer.
    if (vk == KeyEscape) {
        app->search.focused = false;
        cx->win->input = nullptr;
        WindowSelectionClear(cx->win);
    }
    // Whatever the shell did not use is the page's. This is cx.propagate():
    // the shell handles its own chords first and then lets the action carry
    // on, which is how a page's arrows and Enter reach it at all.
    StoryKeyRegistered(app, cx, ev);
}

// The story to open, if one was named on the command line.
static void ParseSlug(int argc, char** argv, char* out, int cap) {
    out[0] = 0;
    if (argc < 2 || !argv[1]) {
        return;
    }
    StrCopyZ(out, cap, argv[1]);
}

int GpuiMain(int argc, char** argv) {
    App* app = AppNew();
    ThemeSet(app, ThemeMode::Light);
    AssetsClear();
    AssetsAddDefaultRoots(Str{});
    AssetsAddRoot(StrL("assets"));

    // A theme out of the registry named in the environment, so a screenshot
    // of one is reproducible the way GPUI_TODAY makes the calendar's today.
    if (const char* themeName = getenv("GPUI_THEME")) {
        ThemeRegistryLoadDir(StrL("themes"));
        const ThemeConfig* cfg = ThemeRegistryFind(Str(themeName));
        if (cfg) {
            ThemeRegistryApply(app, cfg);
            ThemeSet(app, cfg->mode);
        }
    }

    char slug[64] = {};
    ParseSlug(argc, argv, slug, 64);
    Window* win = StoryOpenWindow(app, StoryFromSlug(slug));
    if (!win) {
        AppFree(app);
        return 1;
    }
    // Rust Gallery::set_active_story puts the launch name in the sidebar
    // search box so the list filters to matching titles.
    if (slug[0]) {
        Entity<StoryApp> view;
        view.id = win->root;
        StoryApp* self = view.Get(app);
        const StoryInfo* m = StoryMeta(self->story);
        InputSetValue(&self->search, Str(m->title));
    }
    int rc = AppRun(app);
    AppFree(app);
    return rc;
}
