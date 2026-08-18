#include "Story.h"
#include "gpui.h"

using namespace gpui;

#include <stdarg.h>
#include <stdio.h>

static StoryPageNewFn gNew[StoryCount] = {};
static StoryPageClickFn gClick[StoryCount] = {};

void StoryRegister(int story, StoryPageNewFn create, StoryPageClickFn click) {
    if (story < 0 || story >= StoryCount) {
        return;
    }
    gNew[story] = create;
    gClick[story] = click;
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

El* StoryRenderRegistered(StoryApp* app, Ctx* cx) {
    EntityId page = StoryPageEntity(app, cx);
    if (!page.IsValid()) {
        return StoryComingSoon(cx, app->story);
    }
    return EntityRender(cx->app, cx->win, cx->a, page);
}

void StoryClickRegistered(StoryApp* app, Ctx* cx, int id) {
    int s = app->story;
    if (s < 0 || s >= StoryCount || !gClick[s]) {
        return;
    }
    EntityId page = StoryPageEntity(app, cx);
    void* self = EntityGet(cx->app, page);
    if (!self) {
        return;
    }
    Ctx pageCx = *cx;
    pageCx.self = page;
    gClick[s](self, &pageCx, id);
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
     "A list of terms and their corresponding descriptions."},
    {"dialog", "Dialog",
     "A window overlaid on either the primary window or another dialog "
     "window."},
    {"dropdown-button", "DropdownButton",
     "A button that opens a dropdown menu of actions."},
    {"editor", "Editor",
     "A code editor with syntax highlighting, line numbers, and folding."},
    {"form", "Form", "Building forms with validation and various input types."},
    {"group-box", "GroupBox",
     "A container that groups related content with a title."},
    {"hover-card", "HoverCard",
     "For sighted users to preview content available behind a link."},
    {"icon", "Icon", "Icon display component."},
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
     "Displays an indicator showing the completion progress of a task."},
    {"radio", "Radio", "Choose one option from a set."},
    {"rating", "Rating", "A rating component that allows users to rate items."},
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
     "An input where the user selects a value from within a given range."},
    {"spinner", "Spinner", "A loading spinner."},
    {"status-bar", "StatusBar",
     "A status bar that typically sits at the bottom of the window."},
    {"stepper", "Stepper",
     "A stepper component to display progress through a sequence of steps."},
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
    const Theme& th = ThemeNow();
    // Rust StorySection is an outline GroupBox: title sits above a bordered
    // content pane that centers its children (crates/story/src/lib.rs).
    El* wrap = Div(a)->FlexCol()->Gap(12)->W(kFill);
    El* head = Div(a)->FlexCol()->Gap(4)->W(kFill);
    head->Child(StoryTxt(cx, StoryDup(cx, title), 14, th.mutedFg)->Semibold());
    if (desc && desc[0]) {
        head->Child(StoryTxt(cx, StoryDup(cx, desc), 12, th.mutedFg)->Wrap());
    }
    El* body = Div(a)
                   ->FlexCol()
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
    const Theme& th = ThemeNow();
    return Div(a)
        ->FlexRow()
        ->ItemsStart()
        ->Bg(th.background)
        ->Border(1, th.border)
        ->Radius(th.radius);
}

static El* ToolbarSep(Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
    return Div(a)->W(1)->H(24)->Shrink0()->Bg(th.border);
}

// No Bg on the button: the group paints its background and border first, and
// an opaque child would cover the stroke that straddles the group's edge.
static El* ToolbarDropBtn(Ctx* cx, int id, Str label) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
    return Div(a)
        ->H(24)
        ->PadX(10)
        ->ItemsCenter()
        ->JustifyCenter()
        ->HoverBg(th.muted)
        ->Click(id)
        ->Child(StoryTxt(cx, label, 12, th.foreground));
}

static El* ToolbarCheckRow(Ctx* cx, int id, const char* label, bool on) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
    return Div(a)
        ->H(28)
        ->W(160)
        ->PadX(10)
        ->FlexRow()
        ->Gap(8)
        ->ItemsCenter()
        ->HoverBg(th.muted)
        ->Click(id)
        ->Child(StoryTxt(cx, on ? StrL("\xE2\x9C\x93") : StrL(" "), 12,
                         th.foreground)
                    ->W(14))
        ->Child(StoryTxt(cx, Str(label), 12, th.foreground));
}

static El* ToolbarMenu(Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
    return Div(a)
        ->FlexCol()
        ->PadY(4)
        ->Bg(th.background)
        ->Border(1, th.border)
        ->Radius(th.radius);
}

El* StoryToolbar(Ctx* cx, StoryToolbarState* st) {
    return StoryToolbar(cx, st, nullptr);
}

El* StoryToolbar(Ctx* cx, StoryToolbarState* st, StoryAccordionOptions* opts) {
    Arena* a = cx->a;
    El* row = Div(a)->FlexRow()->W(kFill)->JustifyEnd()->ItemsStart();
    El* group = ToolbarGroup(cx);
    row->Child(group);

    El* sizeTrig = ToolbarDropBtn(
        cx, ClickSizeMenu, StoryFmt(cx, "Size: %s", StorySizeName(st->size)));
    El* sizeMenu = nullptr;
    if (st->sizeMenuOpen) {
        sizeMenu = ToolbarMenu(cx);
        sizeMenu->Child(ToolbarCheckRow(cx, ClickSizeXs, "XSmall",
                                        st->size == UiSize::XSmall));
        sizeMenu->Child(ToolbarCheckRow(cx, ClickSizeSm, "Small",
                                        st->size == UiSize::Small));
        sizeMenu->Child(ToolbarCheckRow(cx, ClickSizeMd, "Medium",
                                        st->size == UiSize::Medium));
        sizeMenu->Child(ToolbarCheckRow(cx, ClickSizeLg, "Large",
                                        st->size == UiSize::Large));
    }
    group->Child(Popup::New(cx, StrL("story-size-menu"), sizeTrig)
                     ->Content(sizeMenu)
                     ->IntoEl());

    if (opts) {
        group->Child(ToolbarSep(cx));
        El* optTrig = ToolbarDropBtn(cx, ClickOptsMenu, StrL("Options"));
        El* optMenu = nullptr;
        if (st->optsOpen) {
            optMenu = ToolbarMenu(cx);
            optMenu->Child(ToolbarCheckRow(cx, ClickAccMultiple, "Multiple",
                                           opts->multiple));
            optMenu
                ->Child(ToolbarCheckRow(cx, ClickAccIcon, "Icons", opts->icon));
            optMenu->Child(ToolbarCheckRow(cx, ClickAccDisabled, "Disabled",
                                           opts->disabled));
            optMenu->Child(ToolbarCheckRow(cx, ClickAccBordered, "Bordered",
                                           opts->bordered));
        }
        group->Child(Popup::New(cx, StrL("story-opts-menu"), optTrig)
                         ->Content(optMenu)
                         ->IntoEl());
    }
    return row;
}

bool StoryToolbarClick(StoryToolbarState* st, int id) {
    switch (id) {
        case ClickEscape:
            st->sizeMenuOpen = false;
            st->optsOpen = false;
            return false;
        case ClickSizeMenu:
            st->sizeMenuOpen = !st->sizeMenuOpen;
            return true;
        case ClickOptsMenu:
            st->optsOpen = !st->optsOpen;
            return true;
        case ClickSizeXs:
            st->size = UiSize::XSmall;
            st->sizeMenuOpen = false;
            return true;
        case ClickSizeSm:
            st->size = UiSize::Small;
            st->sizeMenuOpen = false;
            return true;
        case ClickSizeMd:
            st->size = UiSize::Medium;
            st->sizeMenuOpen = false;
            return true;
        case ClickSizeLg:
            st->size = UiSize::Large;
            st->sizeMenuOpen = false;
            return true;
        default:
            return false;
    }
}

bool StoryAccordionOptionsClick(StoryAccordionOptions* o, int id) {
    switch (id) {
        case ClickAccMultiple:
            o->multiple = !o->multiple;
            return true;
        case ClickAccIcon:
            o->icon = !o->icon;
            return true;
        case ClickAccDisabled:
            o->disabled = !o->disabled;
            return true;
        case ClickAccBordered:
            o->bordered = !o->bordered;
            return true;
        default:
            return false;
    }
}

El* StoryComingSoon(Ctx* cx, int story) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
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

static El* SidebarList(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
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
                      ->Click(ClickStory + i)
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
    const Theme& th = ThemeNow();
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
                  ->Click(ClickSearch)
                  ->FocusId(ClickSearch);
    box->Child(::Input::New(cx, &app->search)->Grow());
    if (app->search.len > 0) {
        box->Child(Div(a)
                       ->W(16)
                       ->H(16)
                       ->ItemsCenter()
                       ->JustifyCenter()
                       ->Shrink0()
                       ->Click(ClickSearchClear)
                       ->Child(IconEl(a, IconName::X, 12)->Fg(th.mutedFg)));
    }
    return box;
}

static El* Sidebar(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
    float w = app->collapsed ? 56.f : 255.f;
    El* side =
        Div(a)->W(w)->H(kFill)->FlexCol()->Bg(th.sidebar)->Border(0, th.border);
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
    const Theme& th = ThemeNow();
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
    const Theme& th = ThemeNow();
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
    const Theme& th = ThemeNow();
    El* root = Div(frame)->FlexCol()->SizeFull()->Bg(th.background);
    El* body = Div(frame)->FlexRow()->Grow()->W(kFill)->MinH(0)->H(kFill);
    body->Child(Sidebar(app, cx));
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

static void OnClick(StoryApp* app, Ctx* cx, const ClickEvent* ev) {
    Window* win = cx->win;
    int id = ev->id;
    if (id == ClickSearch) {
        app->search.focused = true;
        cx->win->input = &app->search;
        AppRequestAnim(win, true);
        return;
    }
    if (id == ClickSearchClear) {
        app->search.buf[0] = 0;
        app->search.len = 0;
        app->search.cursor = 0;
        app->search.focused = false;
        cx->win->input = nullptr;
        return;
    }
    app->search.focused = false;
    cx->win->input = nullptr;
    if (id == ClickCollapse) {
        app->collapsed = !app->collapsed;
        return;
    }
    if (id >= ClickStory && id < ClickStory + StoryCount) {
        app->story = id - ClickStory;
        app->scrollY = 0;
        app->selA = -1;
        app->selB = -1;
        app->selecting = false;
        return;
    }
    StoryClickRegistered(app, cx, id);
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
        StoryClickRegistered(app, cx, ClickEscape);
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
    ThemeSet(ThemeMode::Light);
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
    WindowOnClick(win, ListenTo(view, &OnClick));
    WindowOnKey(win, ListenTo(view, &OnKey));
    WindowOnWheel(win, ListenTo(view, &OnWheel));
    WindowOnMouse(win, ListenTo(view, &OnMouse));
    int rc = AppRun(app);
    AppFree(app);
    return rc;
}
