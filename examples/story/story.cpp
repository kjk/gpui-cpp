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
     "A modal dialog that interrupts the user with important content and "
     "expects a response."},
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
    {"chart", "Chart", "Beautiful charts. Built using GPUI components."},
    {"checkbox", "Checkbox", "Select one or more independent options."},
    {"clipboard", "Clipboard",
     "Copy text or generated values to the clipboard."},
    {"collapsible", "Collapsible",
     "An interactive element that expands/collapses."},
    {"color-picker", "ColorPicker",
     "A color picker that allows users to select a color."},
    {"combobox", "Combobox",
     "Autocomplete input and command palette with a list of suggestions."},
    {"data-table", "DataTable", "Powerful table and datagrids built."},
    {"date-picker", "DatePicker",
     "A date picker component with range and presets."},
    {"description-list", "DescriptionList",
     "Present labels and values in a structured summary."},
    {"dialog", "Dialog",
     "A window overlaid on either the primary window or another dialog "
     "window."},
    {"dropdown-button", "DropdownButton",
     "A button that opens a dropdown menu of actions."},
    {"editor", "Editor",
     "A code editor with syntax highlighting, line numbers, and folding."},
    {"form", "Form", "Building forms with validation and various input types."},
    {"group-box", "GroupBox",
     "A styled container element that with an optional title to groups "
     "related content together."},
    {"hover-card", "HoverCard",
     "For sighted users to preview content available behind a link."},
    {"icon", "Icon", "SVG Icons based on Lucide.dev"},
    {"image", "Image", "Image display with fallbacks."},
    {"input", "Input",
     "Displays a form input field or a component that looks like an input "
     "field."},
    {"kbd", "Kbd", "A tag style to display keyboard shortcuts"},
    {"label", "Label",
     "Display concise text with hierarchy, highlighting, and masking."},
    {"list", "List", "A list of items that can be selected."},
    {"menu", "Menu",
     "Displays a menu to the user — such as a set of actions or functions."},
    {"native-menu", "NativeMenu", "Native application menu bar."},
    {"notification", "Notification",
     "A brief message that appears temporarily."},
    {"number-input", "NumberInput",
     "An input for numeric values with increment and decrement controls."},
    {"otp-input", "OtpInput", "A one-time password input component."},
    {"pagination", "Pagination",
     "Pagination with page navigation, next and previous controls."},
    {"popover", "Popover",
     "Displays rich content in a portal, triggered by a button."},
    {"progress", "Progress",
     "Show task completion with determinate or loading indicators."},
    {"radio", "Radio", "Choose one option from a set."},
    {"rating", "Rating", "A simple interactive star rating component."},
    {"resizable", "Resizable",
     "Accessible resizable panel groups and layouts."},
    {"scrollbar", "Scrollbar",
     "A scrollbar that allows users to scroll content."},
    {"select", "Select",
     "Displays a list of options for the user to pick from."},
    {"separator", "Separator",
     "A separator that can be either vertical or horizontal."},
    {"settings", "Settings", "A settings page with groups and typed fields."},
    {"sheet", "Sheet",
     "Extends the Dialog component to display content that complements the "
     "main content."},
    {"sidebar", "Sidebar",
     "A composable, themeable and customizable sidebar component."},
    {"skeleton", "Skeleton",
     "Use to show a placeholder while content is loading."},
    {"slider", "Slider",
     "Displays a slider control for selecting a value within a range."},
    {"spinner", "Spinner", "A loading spinner."},
    {"status-bar", "StatusBar",
     "A status bar that typically sits at the bottom of the window."},
    {"stepper", "Stepper",
     "A step-by-step process for users to navigate through a series of "
     "steps."},
    {"switch", "Switch", "Turn a setting on or off."},
    {"table", "Table", "A responsive table component."},
    {"tabs", "Tabs",
     "A set of layered sections of content—known as tab panels—that are "
     "displayed one at a time."},
    {"tag", "Tag", "A tag component to categorize or organize items."},
    {"textarea", "Textarea",
     "Displays a form textarea or a component that looks like a textarea."},
    {"theme-colors", "Theme Colors",
     "Theme color tokens used by the components."},
    {"toggle", "Toggle", "Turn an option on or off, alone or in a group."},
    {"tooltip", "Tooltip",
     "A popup that displays information related to an element when the element "
     "receives keyboard focus or the mouse hovers over it."},
    {"tree", "Tree", "A tree view component for hierarchical data."},
    {"virtual-list", "VirtualList",
     "A virtualized list for efficiently rendering large lists."},
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
    _vsnprintf_s(buf, _TRUNCATE, f, args);
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
    El* wrap = Div(a)->FlexCol()->Gap(12)->W(kFill);
    El* head = Div(a)->FlexCol()->Gap(4)->W(kFill);
    head->Child(StoryTxt(cx, StoryDup(cx, title), 14, th.mutedFg)->Semibold());
    if (desc && desc[0]) {
        head->Child(StoryTxt(cx, StoryDup(cx, desc), 12, th.mutedFg)->Wrap());
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
    wrap->Child(head);
    wrap->Child(body);
    return wrap;
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
                           bool on) {
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

El* StoryToolbarCore(Ctx* cx, StoryToolbarState* st,
                     StoryAccordionOptions* opts, Listener onAct) {
    Arena* a = cx->a;
    El* row = Div(a)->FlexRow()->W(kFill)->JustifyEnd()->ItemsStart();
    El* group = ToolbarGroup(cx);
    row->Child(group);

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
    group->Child(Popup::New(cx, StrL("story-size-menu"), sizeTrig)
                     ->Content(sizeMenu)
                     ->IntoEl());

    if (opts) {
        group->Child(ToolbarSep(cx));
        El* optTrig = ToolbarDropBtn(cx, StrL("Options"))
                          ->OnClick(ListenerArg(onAct, ToolbarOpenOpts));
        El* optMenu = nullptr;
        if (st->optsOpen) {
            optMenu = ToolbarMenu(cx);
            optMenu->Child(ToolbarCheckRow(cx, onAct, ToolbarOptMultiple,
                                           "Multiple", opts->multiple));
            optMenu->Child(ToolbarCheckRow(cx, onAct, ToolbarOptIcon, "Icons",
                                           opts->icon));
            optMenu->Child(ToolbarCheckRow(cx, onAct, ToolbarOptDisabled,
                                           "Disabled", opts->disabled));
            optMenu->Child(ToolbarCheckRow(cx, onAct, ToolbarOptBordered,
                                           "Bordered", opts->bordered));
        }
        group->Child(Popup::New(cx, StrL("story-opts-menu"), optTrig)
                         ->Content(optMenu)
                         ->IntoEl());
    }
    return row;
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
    AppRequestAnim(cx->win, true);
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
    WinSize size = WindowSize(win);
    cx->win->paint.selA = app->selA;
    cx->win->paint.selB = app->selB;
    // Pages that own a text field point the window at it from their Render.
    if (app->search.focused) {
        cx->win->input = &app->search;
    }
    AppRequestAnim(win, cx->win->input != nullptr);
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

static void CopyUtf8(HWND hwnd, const char* s, int n) {
    if (!hwnd || !s || n <= 0) {
        return;
    }
    int wn = MultiByteToWideChar(CP_UTF8, 0, s, n, nullptr, 0);
    if (wn <= 0) {
        return;
    }
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)(wn + 1) * sizeof(WCHAR));
    if (!h) {
        return;
    }
    auto* w = (WCHAR*)GlobalLock(h);
    MultiByteToWideChar(CP_UTF8, 0, s, n, w, wn);
    w[wn] = 0;
    GlobalUnlock(h);
    if (!OpenClipboard(hwnd)) {
        GlobalFree(h);
        return;
    }
    EmptyClipboard();
    SetClipboardData(CF_UNICODETEXT, h);
    CloseClipboard();
}

static void OnKey(StoryApp* app, Ctx* cx, const KeyEvent* ev) {
    Window* win = cx->win;
    int vk = ev->vk;
    bool down = ev->down;
    if (ev->ch != 0) {
        OnChar(app, cx, ev);
        return;
    }
    if (!down) {
        return;
    }
    if (vk == 'C' && (GetKeyState(VK_CONTROL) & 0x8000) && app->selA >= 0 &&
        app->selA != app->selB) {
        char buf[8192];
        int n = CopyTextHits(&cx->win->paint, app->selA, app->selB, buf,
                             (int)sizeof(buf));
        if (n > 0) {
            CopyUtf8(cx->win->hwnd, buf, n);
        }
        return;
    }
    if (vk == VK_ESCAPE) {
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
    Window* win = cx->win;
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
    Window* win = cx->win;
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

static void ParseSlug(PWSTR cmd, char* out, int cap) {
    out[0] = 0;
    if (!cmd) {
        return;
    }
    while (*cmd == L' ' || *cmd == L'\t') {
        cmd++;
    }
    if (*cmd == L'"') {
        cmd++;
    }
    int n = 0;
    while (*cmd && *cmd != L' ' && *cmd != L'"' && n < cap - 1) {
        wchar_t c = *cmd++;
        out[n++] = (c < 128) ? (char)c : '?';
    }
    out[n] = 0;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR cmd, int) {
    App* app = AppNew();
    ThemeSet(app, ThemeMode::Light);
    AssetsClear();
    AssetsAddDefaultRoots(Str{});
    AssetsAddRoot(StrL("assets"));

    Entity<StoryApp> view = EntityNew<StoryApp>(app);
    StoryApp* self = view.Get(app);
    char slug[64] = {};
    ParseSlug(cmd, slug, 64);
    self->story = StoryFromSlug(slug);
    // Rust Gallery::set_active_story puts the launch name in the sidebar
    // search box so the list filters to matching titles.
    if (slug[0]) {
        const StoryInfo* m = StoryMeta(self->story);
        strncpy_s(self->search.buf, m->title, _TRUNCATE);
        self->search.len = (int)strlen(self->search.buf);
        self->search.cursor = self->search.len;
    }
    strncpy_s(self->search.placeholder, "Search…", _TRUNCATE);
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
