#include "Story.h"
#include "gpui/Assets.h"

#include <stdarg.h>
#include <stdio.h>

static StoryRenderFn gRender[StoryCount] = {};
static StoryClickFn gClick[StoryCount] = {};

void StoryRegister(int story, StoryRenderFn render, StoryClickFn click) {
    if (story < 0 || story >= StoryCount) {
        return;
    }
    gRender[story] = render;
    gClick[story] = click;
}

El* StoryRenderRegistered(StoryApp* app, Arena* a, WinSize size) {
    int s = app->story;
    if (s >= 0 && s < StoryCount && gRender[s]) {
        return gRender[s](app, a, size);
    }
    return StoryComingSoon(a, s);
}

void StoryClickRegistered(StoryApp* app, int id) {
    int s = app->story;
    if (s >= 0 && s < StoryCount && gClick[s]) {
        gClick[s](app, id);
    }
}

static const StoryInfo kMeta[StoryCount] = {
    {"introduction", "Introduction",
     "UI components for building fantastic desktop application by using GPUI."},
    {"accordion", "Accordion",
     "A vertically stacked set of interactive headings that each reveal a "
     "section of content."},
    {"alert", "Alert",
     "Communicate important status changes without interrupting the user's "
     "workflow."},
    {"alert-dialog", "AlertDialog",
     "A modal dialog that interrupts the user with important content and "
     "expects a response."},
    {"avatar", "Avatar",
     "An image element with a fallback for representing the user."},
    {"badge", "Badge",
     "A red dot that indicates the number of unread messages."},
    {"breadcrumb", "Breadcrumb",
     "Displays the path to the current resource using a hierarchy of links."},
    {"button", "Button",
     "Displays a button or a component that looks like a button."},
    {"calendar", "Calendar", "A calendar of days displayed in a grid."},
    {"chart", "Chart", "Beautiful charts. Built using GPUI components."},
    {"checkbox", "Checkbox",
     "A control that allows the user to toggle between checked and not "
     "checked."},
    {"clipboard", "Clipboard", "A button that copies text to the clipboard."},
    {"collapsible", "Collapsible",
     "An interactive component which expands/collapses a panel."},
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
    {"kbd", "Kbd", "A component to display keyboard shortcuts."},
    {"label", "Label", "Renders an accessible label associated with controls."},
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
    {"radio", "Radio",
     "A set of checkable buttons where only one can be checked at a time."},
    {"rating", "Rating", "A rating component that allows users to rate items."},
    {"resizable", "Resizable",
     "Accessible resizable panel groups and layouts."},
    {"scrollbar", "Scrollbar",
     "A scrollbar that allows users to scroll content."},
    {"select", "Select",
     "Displays a list of options for the user to pick from."},
    {"separator", "Separator", "Visually or semantically separates content."},
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
    {"switch", "Switch",
     "A control that allows the user to toggle between checked and not "
     "checked."},
    {"table", "Table", "A responsive table component."},
    {"tabs", "Tabs",
     "A set of layered sections of content—known as tab panels—that are "
     "displayed one at a time."},
    {"tag", "Tag", "A tag component to categorize or organize items."},
    {"textarea", "Textarea",
     "Displays a form textarea or a component that looks like a textarea."},
    {"theme-colors", "Theme Colors",
     "Theme color tokens used by the components."},
    {"toggle", "Toggle", "A two-state button that can be either on or off."},
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
        if (str::EqI(Str(slug), Str(kMeta[i].slug)) ||
            str::EqI(Str(slug), Str(kMeta[i].title))) {
            return i;
        }
    }
    return StoryWelcome;
}

Str StoryDup(Arena* a, const char* s) {
    return str::Dup(a, Str(s));
}

Str StoryFmt(Arena* a, const char* f, ...) {
    char buf[512];
    va_list args;
    va_start(args, f);
    _vsnprintf_s(buf, _TRUNCATE, f, args);
    va_end(args);
    return str::Dup(a, Str(buf));
}

El* StoryTxt(Arena* a, Str s, float px, Rgba c) {
    return TextEl(a, s)->Font(px)->Fg(c);
}

El* StorySection(Arena* a, const char* title, const char* desc) {
    const Theme& th = ThemeNow();
    El* head = Div(a)->FlexCol()->Gap(4)->W(kFill);
    head->Child(StoryTxt(a, StoryDup(a, title), 14, th.foreground)->Semibold());
    if (desc && desc[0]) {
        head->Child(StoryTxt(a, StoryDup(a, desc), 12, th.mutedFg)->Wrap());
    }
    El* box = Div(a)
                  ->FlexCol()
                  ->Gap(12)
                  ->Pad(16)
                  ->W(kFill)
                  ->Border(1, th.border)
                  ->Radius(th.radius);
    box->Child(head);
    El* body = Div(a)->FlexCol()->Gap(16)->W(kFill)->ItemsStart();
    box->Child(body);
    return box;
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

El* StoryToolbar(Arena* a, StoryApp* app) {
    const Theme& th = ThemeNow();
    const char* labels[] = {"XS", "SM", "MD", "LG"};
    UiSize sizes[] = {UiSize::XSmall, UiSize::Small, UiSize::Medium,
                      UiSize::Large};
    int clicks[] = {ClickSizeXs, ClickSizeSm, ClickSizeMd, ClickSizeLg};
    El* row = Div(a)->FlexRow()->Gap(4)->ItemsCenter();
    for (int i = 0; i < 4; i++) {
        bool on = app->size == sizes[i];
        El* b = Div(a)
                    ->H(24)
                    ->PadX(8)
                    ->ItemsCenter()
                    ->JustifyCenter()
                    ->Radius(4)
                    ->Click(clicks[i])
                    ->Child(StoryTxt(a, Str(labels[i]), 11,
                                     on ? th.primaryFg : th.foreground));
        if (on) {
            b->Bg(th.primary);
        } else {
            b->Border(1, th.border)->HoverBg(th.muted);
        }
        row->Child(b);
    }
    return row;
}

El* StoryComingSoon(Arena* a, int story) {
    const Theme& th = ThemeNow();
    const StoryInfo* m = StoryMeta(story);
    return Div(a)
        ->FlexCol()
        ->Gap(8)
        ->Pad(8)
        ->Child(StoryTxt(a, StoryDup(a, m->title), 16, th.foreground)
                    ->Semibold())
        ->Child(StoryTxt(a, StoryDup(a, "This story is not ported yet."), 13,
                         th.mutedFg));
}

static bool StoryMatches(const StoryInfo* m, const char* q) {
    if (!q || !q[0]) {
        return true;
    }
    return str::ContainsI(Str(m->title), Str(q)) ||
           str::ContainsI(Str(m->slug), Str(q));
}

static El* SidebarList(StoryApp* app, Arena* a) {
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
        El* label = StoryTxt(a, Str(m->title), 13, th.sidebarFg);
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

static El* SearchBox(StoryApp* app, Arena* a) {
    const Theme& th = ThemeNow();
    return Div(a)
        ->H(36)
        ->W(kFill)
        ->PadX(14)
        ->ItemsCenter()
        ->Radius(18)
        ->Bg(th.secondary)
        ->Click(ClickSearch)
        ->FocusId(ClickSearch)
        ->Child(::Input::New(a, &app->search));
}

static El* Sidebar(StoryApp* app, Arena* a) {
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
        names->Child(StoryTxt(a, StrL("GPUI Component"), 14, th.sidebarFg)
                         ->Semibold());
        names->Child(StoryTxt(a, StrL("Component showcase"), 12, th.mutedFg));
        brand->Child(names);
    }
    header->Child(brand);
    if (!app->collapsed) {
        header->Child(SearchBox(app, a));
    }
    side->Child(header);
    El* scroller = Div(a)->Grow()->ClipY()->ScrollY(0)->W(kFill);
    if (!app->collapsed) {
        scroller->Child(SidebarList(app, a));
    }
    side->Child(scroller);
    return side;
}

static El* Header(StoryApp* app, Arena* a) {
    const Theme& th = ThemeNow();
    const StoryInfo* m = StoryMeta(app->story);
    return Div(a)
        ->Pad(16)
        ->FlexCol()
        ->Gap(4)
        ->Shrink0()
        ->BorderB(1, th.border)
        ->Child(StoryTxt(a, Str(m->title), 22, th.foreground)->Semibold())
        ->Child(StoryTxt(a, Str(m->description), 13, th.mutedFg)->Wrap());
}

static El* Footer(StoryApp* app, Arena* a) {
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
                ->Child(StoryTxt(a, StoryFmt(a, "%d components", StoryCount),
                                 12, th.mutedFg))
                ->Child(Div(a)->W(1)->H(12)->Bg(th.border))
                ->Child(StoryTxt(a, Str(m->title), 12, th.mutedFg)))
        ->Child(Div(a)
                    ->FlexRow()
                    ->Gap(12)
                    ->ItemsCenter()
                    ->Child(StoryTxt(a,
                                     ThemeGet() == ThemeMode::Dark
                                         ? StrL("Default Dark")
                                         : StrL("Default Light"),
                                     12, th.mutedFg))
                    ->Child(StoryTxt(a, StrL("v0.5.1"), 12, th.mutedFg)));
}

static El* OnRender(AppHost* host, Arena* frame, WinSize size) {
    auto* app = (StoryApp*)host->user;
    app->hoverId = host->hoverId;
    if (app->search.focused) {
        host->input = &app->search;
    } else if (app->field.focused) {
        host->input = &app->field;
    } else {
        host->input = nullptr;
    }
    AppRequestAnim(host, app->search.focused || app->field.focused);
    const Theme& th = ThemeNow();
    El* root = Div(frame)->FlexCol()->SizeFull()->Bg(th.background);
    El* body = Div(frame)->FlexRow()->Grow()->W(kFill)->MinH(0)->H(kFill);
    body->Child(Sidebar(app, frame));
    El* main = Div(frame)->FlexCol()->Grow()->H(kFill)->MinW(0);
    main->Child(Header(app, frame));
    El* scroller = Div(frame)->Grow()->ClipY()->ScrollY(app->scrollY)->W(kFill);
    scroller->Child(Div(frame)->Pad(16)->W(kFill)->Child(
        StoryRenderRegistered(app, frame, size)));
    main->Child(scroller);
    body->Child(main);
    root->Child(body);
    root->Child(Footer(app, frame));
    return root;
}

static void OnInit(AppHost* host) {
    ThemeSet(ThemeMode::Light);
    AssetsClear();
    AssetsAddDefaultRoots(Str{});
    AssetsAddRoot(StrL("assets"));
    auto* app = (StoryApp*)host->user;
    strncpy_s(app->search.placeholder, "Search…", _TRUNCATE);
    strncpy_s(app->field.placeholder, "Type something…", _TRUNCATE);
    strncpy_s(app->field.buf, "Hello GPUI", _TRUNCATE);
    app->field.len = (int)strlen(app->field.buf);
}

static void OnClick(AppHost* host, int id) {
    auto* app = (StoryApp*)host->user;
    if (id == ClickSearch) {
        app->search.focused = true;
        host->input = &app->search;
        AppRequestAnim(host, true);
        return;
    }
    app->search.focused = false;
    host->input = nullptr;
    if (id == ClickCollapse) {
        app->collapsed = !app->collapsed;
        return;
    }
    if (id == ClickSizeXs) {
        app->size = UiSize::XSmall;
        return;
    }
    if (id == ClickSizeSm) {
        app->size = UiSize::Small;
        return;
    }
    if (id == ClickSizeMd) {
        app->size = UiSize::Medium;
        return;
    }
    if (id == ClickSizeLg) {
        app->size = UiSize::Large;
        return;
    }
    if (id >= ClickStory && id < ClickStory + StoryCount) {
        app->story = id - ClickStory;
        app->scrollY = 0;
        return;
    }
    StoryClickRegistered(app, id);
}

static void OnChar(AppHost* host, u32 cp) {
    auto* app = (StoryApp*)host->user;
    if (app->search.focused) {
        (void)cp;
    }
}

static void OnKey(AppHost* host, int vk, bool down) {
    auto* app = (StoryApp*)host->user;
    if (!down) {
        return;
    }
    if (vk == VK_ESCAPE) {
        app->search.focused = false;
        host->input = nullptr;
        app->dialogOpen = false;
        app->sheetOpen = false;
        app->alertOpen = false;
    }
}

static void OnWheel(AppHost* host, float x, float y, float delta) {
    (void)x;
    (void)y;
    auto* app = (StoryApp*)host->user;
    app->scrollY -= delta;
    if (app->scrollY < 0) {
        app->scrollY = 0;
    }
    if (app->scrollY > 8000) {
        app->scrollY = 8000;
    }
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
    static StoryApp app;
    char slug[64] = {};
    ParseSlug(cmd, slug, 64);
    app.story = StoryFromSlug(slug);

    AppHooks hooks = {};
    hooks.onInit = OnInit;
    hooks.onRender = OnRender;
    hooks.onClick = OnClick;
    hooks.onChar = OnChar;
    hooks.onKey = OnKey;
    hooks.onWheel = OnWheel;
    return RunApp(L"GPUI Component", 1280, 960, hooks, &app);
}
