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

// No Bg on the button: the group paints its background and border first, and
// an opaque child would cover the stroke that straddles the group's edge.
static El* ToolbarDropBtn(Ctx* cx, Str label) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    return Div(a)
        ->H(24)
        ->PadX(10)
        ->ItemsCenter()
        ->JustifyCenter()
        ->HoverBg(th.muted)
        ->Child(StoryTxt(cx, label, 12, th.foreground));
}

static El* ToolbarCheckRow(Ctx* cx, Listener onAct, int act, const char* label,
                           bool on, bool plain = false) {
    // TODO: plain means "no check column" (StoryToolbarOpt::plain, Rust's
    // menu() rather than menu_with_check()); the cell below is still drawn
    // unconditionally.
    (void)plain;
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    return Div(a)
        ->H(28)
        ->W(160)
        ->PadX(10)
        ->FlexRow()
        ->Gap(8)
        ->ItemsCenter()
        ->HoverBg(th.muted)
        ->OnClick(ListenerArg(onAct, act))
        ->Child(StoryTxt(cx, on ? StrL("\xE2\x9C\x93") : StrL(" "), 12,
                         th.foreground)
                    ->W(14))
        ->Child(StoryTxt(cx, Str(label), 12, th.foreground));
}

static El* ToolbarMenu(Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    return Div(a)
        ->FlexCol()
        ->PadY(4)
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
        sizeMenu = ToolbarMenu(cx);
        sizeMenu->Child(ToolbarCheckRow(cx, onAct, ToolbarSizeXs, "XSmall",
                                        st->size == UiSize::XSmall));
        sizeMenu->Child(ToolbarCheckRow(cx, onAct, ToolbarSizeSm, "Small",
                                        st->size == UiSize::Small));
        sizeMenu->Child(ToolbarCheckRow(cx, onAct, ToolbarSizeMd, "Medium",
                                        st->size == UiSize::Medium));
        sizeMenu->Child(ToolbarCheckRow(cx, onAct, ToolbarSizeLg, "Large",
                                        st->size == UiSize::Large));
    }
    return Popup::New(cx, StrL("story-size-menu"), sizeTrig)
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
            optMenu = ToolbarMenu(cx);
            for (int i = 0; i < nrows; i++) {
                optMenu->Child(ToolbarCheckRow(cx, onAct, rows[i].act,
                                               rows[i].label, rows[i].checked,
                                               rows[i].plain));
            }
        }
        group->Child(Popup::New(cx, StrL("story-opts-menu"), optTrig)
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
        menu = ToolbarMenu(cx);
        for (int i = 0; i < nrows; i++) {
            menu->Child(ToolbarCheckRow(cx, onAct, rows[i].act, rows[i].label,
                                        rows[i].checked, rows[i].plain));
        }
    }
    return Popup::New(cx, id, trigger)->Content(menu)->IntoEl();
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
    app->selecting = false;
    Notify(cx);
}

static void FocusSearch(StoryApp* app, Ctx* cx, const ClickEvent*) {
    app->search.focused = true;
    cx->win->input = &app->search;
    Notify(cx);
}

static void ClearSearch(StoryApp* app, Ctx* cx, const ClickEvent*) {
    app->search.buf[0] = 0;
    app->search.len = 0;
    app->search.cursor = 0;
    app->search.focused = false;
    cx->win->input = nullptr;
    Notify(cx);
}

static El* SidebarList(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* list = Div(a)->FlexCol()->Gap(2)->Pad(8);
    const char* q = app->search.buf;
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
    if (app->search.len > 0) {
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
    Window* win = cx->win;
    cx->win->paint.selA = app->selA;
    cx->win->paint.selB = app->selB;
    // Pages that own a text field point the window at it from their Render.
    if (app->search.focused) {
        cx->win->input = &app->search;
    }
    if (cx->win->input != nullptr) {
        WindowCaretStart(win);
    } else {
        WindowCaretStop(win);
    }
    const Theme& th = cx->theme();
    El* root = Div(frame)->FlexCol()->SizeFull()->Bg(th.background);
    El* body = Div(frame)->FlexRow()->Grow()->W(kFill)->MinH(0)->H(kFill);
    body->Child(Sidebar(app, cx));
    // The resizable handle reads as a 1px rule.
    body->Child(Div(frame)->W(1)->H(kFill)->Shrink0()->Bg(th.border));
    El* main = Div(frame)->FlexCol()->Grow()->H(kFill)->MinW(0);
    main->Child(Header(app, cx));
    El* scroller = Div(frame)
                       ->FlexCol()
                       ->Grow()
                       ->MinH(0)
                       ->ClipY()
                       ->ScrollY(app->scrollY)
                       ->ScrollId(1)
                       ->W(kFill);
    scroller->Child(
        Div(frame)->Pad(16)->W(kFill)->Child(StoryRenderRegistered(app, cx)));
    main->Child(scroller);
    body->Child(main);
    root->Child(body);
    root->Child(Footer(app, cx));
    return root;
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
    if (vk == KeyC && ev->ctrl && app->selA >= 0 && app->selA != app->selB) {
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
        app->selecting = false;
        // Let the page close whatever it has open.
        StoryKeyRegistered(app, cx, ev);
    }
}

static void OnWheel(StoryApp* app, Ctx* cx, const WheelEvent* ev) {
    float x = ev->x;
    float y = ev->y;
    float delta = ev->delta;
    const ScrollRect* pane = HitScrollRect(&cx->win->paint, x, y);
    float* off = &app->scrollY;
    float maxS = 8000.f;
    if (pane) {
        if (pane->id == 2) {
            off = &app->sideScrollY;
        }
        maxS = pane->contentH - pane->h;
        if (maxS < 0) {
            maxS = 0;
        }
    }
    *off -= delta;
    if (*off < 0) {
        *off = 0;
    }
    if (*off > maxS) {
        *off = maxS;
    }
}

static void OnMouse(StoryApp* app, Ctx* cx, const MouseEvent* ev) {
    float x = ev->x;
    float y = ev->y;
    int button = ev->button;
    if (ev->kind == MouseKind::Up) {
        app->selecting = false;
        return;
    }
    if (ev->kind == MouseKind::Move) {
        if (!app->selecting) {
            return;
        }
        int moveOff = TextHitOffsetAt(&cx->win->paint, x, y, true);
        if (moveOff >= 0) {
            app->selB = moveOff;
        }
        return;
    }
    if (button != 1) {
        return;
    }
    int off = TextHitOffsetAt(&cx->win->paint, x, y, false);
    if (off >= 0) {
        app->selA = off;
        app->selB = off;
        app->selecting = true;
        return;
    }
    app->selA = -1;
    app->selB = -1;
    app->selecting = false;
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
        StrCopyZ(self->search.buf, (int)sizeof(self->search.buf), m->title);
        self->search.len = (int)strlen(self->search.buf);
        self->search.cursor = self->search.len;
    }
    StrCopyZ(self->search.placeholder, (int)sizeof(self->search.placeholder),
             "Search…");
    Window* win = WindowOpenView(app, StrL("GPUI Component"), 1280, 960,
                                 view.id, WinOpts{});
    WindowOnUnhandledClick(win, ListenTo(view, &OnUnhandledClick));
    WindowOnKey(win, ListenTo(view, &OnKey));
    WindowOnWheel(win, ListenTo(view, &OnWheel));
    WindowOnMouse(win, ListenTo(view, &OnMouse));
    int rc = AppRun(app);
    AppFree(app);
    return rc;
}
