#include "Story.h"

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
    bool scrollOpen = false;
    int scrollItems = 0;
    // Which context-menu area was right-clicked, and where.
    int ctxArea = -1;
    float ctxX = 0;
    float ctxY = 0;

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

static void OpenScroll(MenuStory* self, Ctx* cx, const ClickEvent*,
                       intptr_t items) {
    self->scrollOpen = !(self->scrollOpen && self->scrollItems == (int)items);
    self->scrollItems = (int)items;
    Notify(cx);
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
    m->MenuWithKbd(StrL("Copy"), StrL("Ctrl+C"));
    m->MenuWithKbd(StrL("Cut"), StrL("Ctrl+X"));
    m->MenuWithKbd(StrL("Paste"), StrL("Ctrl+V"));
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
        ->MenuWithKbd(StrL("Cut"), StrL("Ctrl+X"))
        ->MenuWithKbd(StrL("Copy"), StrL("Ctrl+C"))
        ->MenuWithKbd(StrL("Paste"), StrL("Ctrl+V"));
}

// The "PopupMenu" key context: the arrows walk the rows, Enter takes one,
// Escape closes the submenu and then the menu.
void MenuStory::OnKey(MenuStory* self, Ctx* cx, const KeyEvent* ev) {
    if (!ev->down) {
        return;
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

static void ToggleEdit(MenuStory* self, Ctx* cx, const ClickEvent*) {
    (void)self;
    Entity<PopupMenuState> st =
        component::PopupMenuStateFor(cx, StrL("popup-menu-1"));
    PopupMenuState* s = st.Get(cx);
    if (!s) {
        return;
    }
    if (s->open) {
        PopupMenuDismiss(s, cx);
    } else {
        PopupMenuOpen(s, cx);
    }
}

// A right press inside one of the areas opens that area's menu where the
// pointer is, which is what ContextMenuExt does.
static void OnAreaDown(MenuStory* self, Ctx* cx, const MouseDownEvent* ev,
                       intptr_t area) {
    if (ev->button != MouseButton::Right) {
        return;
    }
    self->ctxArea = (int)area;
    // Where the press landed inside the area, so the menu can hang off the
    // area's own box rather than the window's.
    self->ctxX = ev->x - ev->el.x;
    self->ctxY = ev->y - ev->el.y;
    Entity<PopupMenuState> st = component::PopupMenuStateFor(
        cx, StoryFmt(cx, "context-menu-%d", (int)area));
    PopupMenuState* s = st.Get(cx);
    if (s) {
        PopupMenuOpen(s, cx);
    }
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
    box->Id(StoryFmt(cx, "ctx-area-%d", area))
        ->Click(HashClickId(StoryFmt(cx, "ctx-area-%d", area)))
        ->OnMouseDown(Listen(cx, &OnAreaDown, area));
    if (self->ctxArea == area) {
        Entity<PopupMenuState> st = component::PopupMenuStateFor(
            cx, StoryFmt(cx, "context-menu-%d", area));
        PopupMenuState* s = st.Get(cx);
        if (s && s->open) {
            component::PopupMenu* m = ContextMenu(cx, area);
            s->onConfirm = Listen(cx, &OnContextItem);
            box->Child(m->IntoEl()
                           ->Absolute()
                           ->Left(self->ctxX)
                           ->Top(self->ctxY)
                           ->Deferred());
        }
    }
    return box;
}

El* MenuStory::Render(MenuStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* popup = StorySection(
        cx, "Popup Menu",
        "Supports actions, links, checks, icons, custom rows, and nested "
        "menus.");
    El* editWrap = Div(a)->FlexCol();
    editWrap->Child(component::Button::New(cx, StrL("edit-menu"))
                        ->Outline()
                        ->Label(StrL("Edit"))
                        ->OnClick(Listen(cx, &ToggleEdit))
                        ->IntoEl());
    component::PopupMenu* edit = EditMenu(self, cx);
    PopupMenuState* editState = edit->state.Get(cx);
    if (editState) {
        editState->onConfirm = Listen(cx, &OnEditItem);
        if (editState->open) {
            editWrap
                ->Child(edit->IntoEl()->AnchorBelow(4)->Left(0)->Deferred());
        }
    }
    StorySectionAdd(popup, editWrap);
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
    El* longWrap = Div(a)->FlexCol();
    longWrap->Child(component::Button::New(cx, StrL("scroll-50"))
                        ->Outline()
                        ->Label(StrL("Scrollable Menu (50 items)"))
                        ->OnClick(Listen(cx, &OpenScroll, 50))
                        ->IntoEl());
    El* shortWrap = Div(a)->FlexCol();
    shortWrap->Child(component::Button::New(cx, StrL("scroll-5"))
                         ->Outline()
                         ->Label(StrL("Scrollable Menu (5 items)"))
                         ->OnClick(Listen(cx, &OpenScroll, 5))
                         ->IntoEl());
    if (self->scrollOpen) {
        component::PopupMenu* m =
            component::PopupMenu::New(cx, StrL("scroll-menu"));
        int shown = self->scrollItems > 12 ? 12 : self->scrollItems;
        for (int i = 1; i <= shown; i++) {
            m->Menu(StoryFmt(cx, "Item %d", i));
        }
        El* menu = m->IntoEl()->AnchorBelow(4)->Left(0)->Deferred();
        (self->scrollItems == 5 ? shortWrap : longWrap)->Child(menu);
    }
    scrollRow->Child(longWrap)->Child(shortWrap);
    StorySectionAdd(scroll, scrollRow);
    page->Child(scroll);
    return page;
}

STORY_PAGE_KEYS(StoryMenu, MenuStory);
