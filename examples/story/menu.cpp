#include "Story.h"

// Kbd::format: a menu shows the shortcut the way the platform spells it,
// rather than a string that only reads right on one of them.
static Str Chord(Ctx* cx, const char* key) {
    component::Keystroke k;
    k.ctrl = true;
    k.key = Str(key);
    return component::KbdFormatStr(cx, k);
}

// The rows of the Edit menu, in the order the Rust story builds them. The
// listener carries the row, and the page says what each one means.
enum {
    EditAbout = 0,
    EditHandleClick,
    EditCopy,
    EditCut,
    EditPaste,
    EditCheckSide,
    EditSearch,
    EditCustom,
    EditLinks
};

struct MenuStory {
    Str message = {};
    bool checkSideRight = false;
    // AppMenuBar is an entity in Rust too — it is what knows which title is
    // open.
    Entity<component::AppMenuBarState> bar = {};
    bool seeded = false;

    ~MenuStory() { StrFree(message); }
    static El* Render(MenuStory* self, Ctx* cx);
    static void OnKey(MenuStory* self, Ctx* cx, const KeyEvent* ev);
};

static void SetMessage(MenuStory* self, Ctx* cx, const char* fmtStr,
                       const char* what) {
    StrFree(self->message);
    self->message = StrDup(fmt(fmtStr, Str(what)));
    Notify(cx);
}

// The menu reports which row was taken; the page decides what that means,
// which is what a Rust action dispatched from the menu ends up doing.
static void OnEditItem(MenuStory* self, Ctx* cx, const ClickEvent*,
                       intptr_t ix) {
    static const char* kNames[] = {"About",  "Handle Click",  "Copy",
                                   "Cut",    "Paste",         "Check Side",
                                   "Search", "Custom Element"};
    if (ix == EditCheckSide) {
        self->checkSideRight = !self->checkSideRight;
        Notify(cx);
        return;
    }
    if (ix >= 0 && ix < (intptr_t)(sizeof(kNames) / sizeof(kNames[0]))) {
        SetMessage(self, cx, "You have clicked %s", kNames[ix]);
    }
}

static void OnContextItem(MenuStory* self, Ctx* cx, const ClickEvent*,
                          intptr_t ix) {
    static const char* kNames[] = {"Cut", "Copy", "Paste"};
    if (ix >= 0 && ix < 3) {
        SetMessage(self, cx, "Context menu: %s", kNames[ix]);
    }
}

// The Edit menu, built the same way every frame so its keyed state and the
// masks the keys walk describe the same rows.
static component::PopupMenu* EditMenu(MenuStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    component::PopupMenu* m =
        component::PopupMenu::New(cx, StrL("popup-menu-1"))
            ->MinW(250)
            ->CheckSide(self->checkSideRight ? Side::Right : Side::Left);
    m->Menu(StrL("About"));
    m->Separator();
    m->Menu(StrL("Handle Click"));
    m->Separator();
    m->MenuWithKbd(StrL("Copy"), Chord(cx, "c"));
    m->MenuWithKbd(StrL("Cut"), Chord(cx, "x"));
    m->MenuWithKbd(StrL("Paste"), Chord(cx, "v"));
    m->Separator();
    m->MenuWithCheck(self->checkSideRight ? StrL("Check Side Right")
                                          : StrL("Check Side Left"),
                     true);
    m->Separator();
    m->Menu(StrL("Search"), IconName::Search);
    m->Separator();
    // PopupMenuItem::element: a row that renders its own content.
    El* custom = Div(a)->FlexCol();
    custom->Child(StoryTxt(cx, StrL("Custom Element"), 14, th.foreground));
    custom->Child(StoryTxt(cx, StrL("This is sub-title"), 12, th.mutedFg));
    m->Element(custom);
    m->Separator();
    m->Submenu(StrL("Links"),
               component::PopupMenu::New(cx, StrL("popup-menu-1-links"))
                   ->Menu(StrL("GPUI"))
                   ->Menu(StrL("Zed"))
                   ->Separator()
                   ->Menu(StrL("Crates")));
    return m;
}

// The three-row menu each context area opens.
static component::PopupMenu* ContextMenu(Ctx* cx, int area) {
    return component::PopupMenu::New(cx, StoryFmt(cx, "context-menu-%d", area))
        ->MenuWithKbd(StrL("Cut"), Chord(cx, "x"))
        ->MenuWithKbd(StrL("Copy"), Chord(cx, "c"))
        ->MenuWithKbd(StrL("Paste"), Chord(cx, "v"));
}

// The "PopupMenu" key context: the arrows walk the rows, Enter takes one,
// Escape closes the submenu and then the menu.
void MenuStory::OnKey(MenuStory* self, Ctx* cx, const KeyEvent* ev) {
    if (!ev->down) {
        return;
    }
    // The menu bar takes the arrows and Escape while one of its titles is
    // open, which is on_move_left / on_move_right / on_cancel.
    component::AppMenuBarState* bar = self->bar.Get(cx);
    if (bar && bar->selected >= 0) {
        if (ev->vk == KeyLeft) {
            component::AppMenuBarSelect(
                bar, cx, component::AppMenuBarPrevIndex(bar->selected, 3));
            return;
        }
        if (ev->vk == KeyRight) {
            component::AppMenuBarSelect(
                bar, cx, component::AppMenuBarNextIndex(bar->selected, 3));
            return;
        }
        if (ev->vk == KeyEscape) {
            component::AppMenuBarSelect(bar, cx, -1);
            return;
        }
    }
    component::PopupMenu* m = EditMenu(self, cx);
    PopupMenuState* st = m->state.Get(cx);
    if (!st || !st->open) {
        return;
    }
    bool clickable[32] = {};
    bool submenu[32] = {};
    m->Masks(clickable, submenu);
    PopupMenuPerform(st, cx, PopupMenuActionForKey(ev->vk, st->side), clickable,
                     submenu, m->n);
}

static El* ContextArea(MenuStory* self, Ctx* cx, int area, Str title,
                       Str hint) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* box = Div(a)
                  ->FlexCol()
                  ->W(kFill)
                  ->Gap(4)
                  ->PadY(24)
                  ->ItemsCenter()
                  ->JustifyCenter()
                  ->Radius(th.radius)
                  ->Border(1, th.border)
                  ->Dashed();
    box->Child(StoryTxt(cx, title, 16, th.foreground));
    if (hint.s) {
        box->Child(StoryTxt(cx, hint, 14, th.mutedFg));
    }
    // ContextMenuExt: the right press, the position and the menu over it are
    // all the component's.
    component::PopupMenu* menu = ContextMenu(cx, area);
    PopupMenuState* st = menu->state.Get(cx);
    if (st) {
        st->onConfirm = Listen(cx, &OnContextItem);
    }
    (void)self;
    return component::ContextMenu::New(cx, StoryFmt(cx, "ctx-area-%d", area))
        ->Child(box)
        ->Menu(menu)
        ->IntoEl();
}

El* MenuStory::Render(MenuStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    if (!self->seeded) {
        self->seeded = true;
        self->bar = EntityNewState<component::AppMenuBarState>(cx->app);
    }
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    // AppMenuBar: one title open at a time, and moving over another switches.
    El* barSec = StorySection(cx, "Menu Bar",
                              "The row of menus across the top of a window.");
    component::AppMenuBar* bar =
        component::AppMenuBar::New(cx, StrL("app-menu-bar"), self->bar);
    bar->Menu(StrL("File"), component::PopupMenu::New(cx, StrL("bar-file"))
                                ->MenuWithKbd(StrL("New"), Chord(cx, "n"))
                                ->MenuWithKbd(StrL("Open"), Chord(cx, "o"))
                                ->Separator()
                                ->MenuWithKbd(StrL("Quit"), Chord(cx, "q")));
    bar->Menu(StrL("Edit"), component::PopupMenu::New(cx, StrL("bar-edit"))
                                ->MenuWithKbd(StrL("Copy"), Chord(cx, "c"))
                                ->MenuWithKbd(StrL("Cut"), Chord(cx, "x"))
                                ->MenuWithKbd(StrL("Paste"), Chord(cx, "v")));
    bar->Menu(StrL("View"), component::PopupMenu::New(cx, StrL("bar-view"))
                                ->MenuWithCheck(StrL("Status Bar"), true)
                                ->MenuWithCheck(StrL("Sidebar"), false));
    StorySectionAdd(barSec, bar->IntoEl());
    page->Child(barSec);

    El* popup = StorySection(
        cx, "Popup Menu",
        "Supports actions, links, checks, icons, custom rows, and nested "
        "menus.");
    component::PopupMenu* edit = EditMenu(self, cx);
    PopupMenuState* editState = edit->state.Get(cx);
    if (editState) {
        editState->onConfirm = Listen(cx, &OnEditItem);
    }
    // DropdownMenu: the trigger, the menu under it, and the click that opens
    // it are the component's.
    StorySectionAdd(popup,
                    component::DropdownMenu::New(cx, StrL("edit-menu"))
                        ->Trigger(component::Button::New(cx, StrL("edit-btn"))
                                      ->Outline()
                                      ->Label(StrL("Edit"))
                                      ->IntoEl())
                        ->Menu(edit)
                        ->IntoEl());
    if (self->message.s) {
        StorySectionAdd(popup, StoryTxt(cx, self->message, 14, th.mutedFg));
    }
    page->Child(popup);

    El* ctxSec =
        StorySection(cx, "Context Menu",
                     "Different regions can provide their own right-click "
                     "actions.");
    El* areas = Div(a)->FlexCol()->W(kFill)->Gap(16);
    areas->Child(ContextArea(
        self, cx, 0, StrL("Right click to open ContextMenu"),
        StrL("You can right click anywhere in this area to open the context "
             "menu.")));
    areas->Child(ContextArea(
        self, cx, 1, StrL("Here is another area with context menu."), Str{}));
    areas->Child(ContextArea(self, cx, 2, StrL("ContextMenu area 1"), Str{}));
    StorySectionAdd(ctxSec, areas);
    page->Child(ctxSec);

    El* scroll = StorySection(
        cx, "Scrollable", "A long menu keeps its height and scrolls its rows.");
    El* scrollRow = Div(a)->FlexRow()->Gap(16)->ItemsStart();
    for (int which = 0; which < 2; which++) {
        int items = which == 0 ? 50 : 5;
        component::PopupMenu* m = component::PopupMenu::New(
            cx, StoryFmt(cx, "scroll-menu-%d", items));
        int shown = items > 12 ? 12 : items;
        for (int i = 1; i <= shown; i++) {
            m->Menu(StoryFmt(cx, "Item %d", i));
        }
        scrollRow->Child(
            component::DropdownMenu::New(cx, StoryFmt(cx, "scroll-%d", items))
                ->Trigger(component::Button::New(
                              cx, StoryFmt(cx, "scroll-btn-%d", items))
                              ->Outline()
                              ->Label(StoryFmt(cx, "Scrollable Menu (%d items)",
                                               items))
                              ->IntoEl())
                ->Menu(m)
                ->IntoEl());
    }
    StorySectionAdd(scroll, scrollRow);
    page->Child(scroll);
    return page;
}

STORY_PAGE_KEYS(StoryMenu, MenuStory);
