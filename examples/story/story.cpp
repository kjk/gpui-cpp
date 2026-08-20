#include "Story.h"
#include "gpui.h"

using namespace gpui;

#include <stdarg.h>
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
    {"tree", "Tree", "A tree view component for hierarchical data."},
    {"virtual-list", "VirtualList",
     "Add vertical or horizontal, or both scrollbars to a "
     "container, and use virtual_list to render a large number of "
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

Str StoryFmt(Ctx* cx, const char* f, ...) {
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
    // section(): h_flex().w_full().flex_wrap().justify_center().items_center()
    // .gap_4() inside the pane.
    El* body = Div(a)
                   ->FlexRow()
                   ->FlexWrap()
                   ->Gap(16)
                   ->Pad(16)
                   ->W(kFill)
                   ->Border(1, th.border)
                   ->Radius(th.radius)
                   ->ItemsCenter()
                   ->JustifyCenter();
    headRow->Child(head);
    wrap->Child(headRow);
    wrap->Child(body);
    return wrap;
}

El* StorySectionSubTitle(El* section, El* sub) {
    if (!section || !sub || !section->first) {
        return section;
    }
    section->first->Child(sub);
    return section;
}

El* StorySectionAdd(El* section, El* child) {
    if (!section || !child) {
        return section;
    }
    El* body = section->first;
    while (body && body->next) {
        body = body->next;
    }
    if (body) {
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
        ->Bg(th.background)
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
        ->HoverBg(th.muted)
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
                  ->HoverBg(th.accent);
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
static El* ToolbarMenu(Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    return Div(a)
        ->FlexCol()
        ->Pad(4)
        ->Gap(2)
        ->Bg(th.background)
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
    app->selA = -1;
    app->selB = -1;
    TextSelectionClear(&app->sel);
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
        El* label = StoryTxt(cx, Str(m->title), 13, th.sidebarFg);
        if (on) {
            label->Semibold();
            row->Bg(th.secondary);
        } else {
            row->HoverBg(th.secondary);
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
                  ->Bg(th.secondary)
                  ->OnClick(Listen(cx, &FocusSearch))
                  ->FocusId(ClickSearch);
    box->Child(::Input::New(cx, &app->search)->Grow());
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
    El* side = Div(a)->W(w)->H(kFill)->FlexCol()->Bg(th.sidebar);
    El* header = Div(a)->FlexCol()->Pad(12)->Gap(16);
    El* brand = Div(a)->FlexRow()->Gap(10)->ItemsCenter();
    El* logo = Div(a)
                   ->W(32)
                   ->H(32)
                   ->Radius(8)
                   ->Bg(th.primary)
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
                       ->Grow()
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
        ->HoverBg(th.muted)
        ->Child(text);
}

// The window's notification list. Rust hangs it off the window — Root renders
// it and `window.notifications(cx)` reads it — so it is the app's here rather
// than the notification page's, and a notification outlives leaving that page.
Entity<component::NotificationListState> StoryNotifications(Ctx* cx) {
    Entity<StoryApp> app;
    app.id = cx->win->root;
    StoryApp* self = app.Get(cx);
    return self ? self->notifications
                : Entity<component::NotificationListState>{};
}

static int StoryNotificationCount(Ctx* cx) {
    component::NotificationListState* st = StoryNotifications(cx).Get(cx);
    return st ? st->n : 0;
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
    // Rust offers a third, Scrolling — shown while scrolling and fading out
    // after two idle seconds — which is not ported.
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
                      ->IntoEl())
        ->Menu(menu)
        // Anchor::TopRight: the menu's right edge lines up with the button's,
        // which is what keeps it on screen at the corner of the window.
        ->AnchorRight()
        ->IntoEl();
}

static void OnGithub(StoryApp*, Ctx*, const ClickEvent*) {
    OpenUrl(StrL("https://github.com/longbridge/gpui-component"));
}

static El* StoryTitleBar(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;

    El* menus = Div(a)
                    ->FlexRow()
                    ->H(kFill)
                    ->ItemsCenter()
                    ->Child(StoryTitleMenuItem(cx, "GPUI Component", true))
                    ->Child(StoryTitleMenuItem(cx, "Edit", false))
                    ->Child(StoryTitleMenuItem(cx, "Window", false))
                    ->Child(StoryTitleMenuItem(cx, "Help", false));
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
                        ->IntoEl())
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
                                ->IntoEl())
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
        ->Bg(th.titleBar)
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
                    ->Child(StoryTxt(cx,
                                     ThemeGet() == ThemeMode::Dark
                                         ? StrL("Default Dark")
                                         : StrL("Default Light"),
                                     12, th.mutedFg))
                    ->Child(StoryTxt(cx, StrL("v0.5.1"), 12, th.mutedFg)));
}

El* StoryApp::Render(StoryApp* app, Ctx* cx) {
    Arena* frame = cx->a;
    // did_hit_text gates the whole selection: a gesture that never touched a
    // glyph shows nothing, however far it dragged.
    bool publishes = TextSelectionPublishes(&app->sel);
    cx->win->paint.selA = publishes ? app->selA : -1;
    cx->win->paint.selB = app->selB;
    // Pages that own a text field point the window at it from their Render.
    if (app->search.focused) {
        cx->win->input = &app->search;
    }
    const Theme& th = cx->theme();
    if (!app->seeded) {
        app->seeded = true;
        app->notifications =
            EntityNewState<component::NotificationListState>(cx->app);
        // Rust spawns a task that advances the list every 50 ms; a window
        // timer is the same clock.
        app->notifyTimer = WindowSetInterval(
            cx->win, component::kNotificationTickMs,
            ListenTo(app->notifications,
                     &component::NotificationListState::OnTick));
    }
    // The window's outermost view is a Root, which is what Rust puts under
    // every window: the page, and over it the layers the window owns.
    El* root = Div(frame)->FlexCol()->SizeFull();
    if (cx->win->opts.clientTitleBar) {
        root->Child(StoryTitleBar(app, cx));
    }
    El* body = Div(frame)->FlexRow()->Grow()->W(kFill)->MinH(0)->H(kFill);
    body->Child(Sidebar(app, cx));
    // The resizable handle reads as a 1px rule. Rust anchors it over the
    // boundary rather than in the flow, so the content starts where the
    // sidebar ends; a border here is painted inside the box without taking
    // space from it, which puts the rule in the same place.
    El* main =
        Div(frame)->FlexCol()->Grow()->H(kFill)->MinW(0)->BorderL(1, th.border);
    main->Child(Header(app, cx));
    El* scroller = Div(frame)
                       ->FlexCol()
                       ->Grow()
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
        ->Notifications(component::NotificationList::New(cx, app->notifications)
                            ->IntoEl())
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
    if (vk == KeyC && ev->ctrl && TextSelectionPublishes(&app->sel) &&
        app->selA >= 0 && app->selA != app->selB) {
        char buf[8192];
        int n = CopyTextHits(&cx->win->paint, app->selA, app->selB, buf,
                             (int)sizeof(buf));
        if (n > 0) {
            ClipboardSetText(cx->win, Str(buf, n));
        }
        return;
    }
    if (vk == KeyEscape) {
        app->search.focused = false;
        cx->win->input = nullptr;
        app->selA = -1;
        app->selB = -1;
        TextSelectionClear(&app->sel);
    }
    // Whatever the shell did not use is the page's. This is cx.propagate():
    // the shell handles its own chords first and then lets the action carry
    // on, which is how a page's arrows and Enter reach it at all.
    StoryKeyRegistered(app, cx, ev);
}

static void OnMouseUp(StoryApp* app, Ctx* cx, const MouseUpEvent*) {
    (void)cx;
    TextSelectionEnd(&app->sel);
}

static void OnMouseMove(StoryApp* app, Ctx* cx, const MouseMoveEvent* ev) {
    if (!app->sel.selecting) {
        return;
    }
    // did_hit_text: whether *this* point is on a glyph, which is the strict
    // hit; the offset the selection extends to is the nearest one either way,
    // so a drag into the margin keeps running along the line.
    TextSelectionExtend(
        &app->sel, TextHitOffsetAt(&cx->win->paint, ev->x, ev->y, false) >= 0);
    int moveOff = TextHitOffsetAt(&cx->win->paint, ev->x, ev->y, true);
    if (moveOff >= 0) {
        app->selB = moveOff;
    }
}

static void OnMouseDown(StoryApp* app, Ctx* cx, const MouseDownEvent* ev) {
    float x = ev->x;
    float y = ev->y;
    if (ev->button != MouseButton::Left) {
        return;
    }
    // Two clicks take the word under the pointer, three the whole run —
    // points_for_multi_click, in text_selection.rs.
    int wordA = 0;
    int wordB = 0;
    if (TextMultiClickRange(&cx->win->paint, x, y, ev->clickCount, &wordA,
                            &wordB)) {
        app->selA = wordA;
        app->selB = wordB;
        // A multi-click lands on a word, so it has hit text by definition;
        // Rust sets did_hit_text outright on that path.
        TextSelectionBegin(&app->sel, true);
        TextSelectionEnd(&app->sel);
        return;
    }
    // A press in the margin still begins a gesture: Rust takes the flag from
    // `anchor.inside_text || endpoint.inside_text`, so dragging from beside a
    // paragraph into it selects. The anchor is the nearest offset; whether it
    // was on a glyph is what decides if anything is published.
    int anchor = TextHitOffsetAt(&cx->win->paint, x, y, true);
    if (anchor >= 0) {
        app->selA = anchor;
        app->selB = anchor;
        TextSelectionBegin(&app->sel,
                           TextHitOffsetAt(&cx->win->paint, x, y, false) >= 0);
        return;
    }
    app->selA = -1;
    app->selB = -1;
    TextSelectionClear(&app->sel);
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

    Entity<StoryApp> view = EntityNew<StoryApp>(app);
    StoryApp* self = view.Get(app);
    char slug[64] = {};
    ParseSlug(argc, argv, slug, 64);
    self->story = StoryFromSlug(slug);
    // Rust Gallery::set_active_story puts the launch name in the sidebar
    // search box so the list filters to matching titles.
    if (slug[0]) {
        const StoryInfo* m = StoryMeta(self->story);
        InputSetValue(&self->search, Str(m->title));
    }
    InputSetPlaceholder(&self->search, StrL("Search…"));
    // create_new_window_with_size passes 1600x1200; WindowOpen caps it at
    // 85% of the display and centers it.
    WinOpts opts = {};
    // TitleBar::window_options(): the story owns its title bar on every
    // platform. macOS keeps the traffic lights over a transparent one,
    // Windows and X11 have none, so component::TitleBar draws the minimize /
    // maximize / close controls there itself.
    opts.clientTitleBar = true;
    Window* win = WindowOpenView(app, StrL("GPUI Component C++"), 1600, 1200,
                                 view.id, opts);
    WindowOnUnhandledClick(win, ListenTo(view, &OnUnhandledClick));
    WindowOnKey(win, ListenTo(view, &OnKey));
    WindowOnMouseDown(win, ListenTo(view, &OnMouseDown));
    WindowOnMouseUp(win, ListenTo(view, &OnMouseUp));
    WindowOnMouseMove(win, ListenTo(view, &OnMouseMove));
    int rc = AppRun(app);
    AppFree(app);
    return rc;
}
