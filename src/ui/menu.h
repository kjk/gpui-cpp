/* Themed menu — crates/ui/src/menu */

#include "ui/sizing.h"
#include "base/popup_menu.h"

namespace gpui {

namespace component {

// PopupMenuItem, the enum: a row is a real item, a rule between groups, or a
// heading that is not clickable.
enum class MenuItemKind : uint8_t {
    Item,
    Separator,
    Label
};

struct PopupMenu;

// The keyboard reads the same rows off the state, so one bound covers both.
constexpr int kPopupMenuMaxItems = kPopupMenuMaxRows;

struct MenuItem {
    MenuItemKind kind = MenuItemKind::Item;
    Str label = {};
    IconName icon = IconName::None;
    // The keystroke shown on the right, which is Rust's Kbd of the item's
    // action.
    Str kbd = {};
    bool checked = false;
    bool disabled = false;
    bool isLink = false;
    Str href = {};
    // A row that opens onto another menu instead of running.
    PopupMenu* submenu = nullptr;
    // An item that renders its own content (PopupMenuItem::ElementItem).
    El* element = nullptr;
};

struct PopupMenu {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<PopupMenuState> state = {};
    MenuItem items[kPopupMenuMaxItems] = {};
    int n = 0;
    UiSize size = UiSize::Medium;
    float minW = 128;
    float maxH = 450;
    bool scrollable = false;
    bool externalLinkIcon = true;
    // check_side: which edge a checked row's tick sits on.
    Side checkSide = Side::Left;

    // The state behind one menu id. Rust's `window.use_keyed_state(id)`, so a
    // page gets a menu's selection and submenu without declaring a field; a
    // caller that owns the state itself passes it instead.
    static PopupMenu* New(Ctx* cx, Str id);
    static PopupMenu* New(Ctx* cx, Str id, Entity<PopupMenuState> state);
    PopupMenu* Menu(Str label, IconName icon = IconName::None);
    PopupMenu* MenuWithCheck(Str label, bool checked);
    PopupMenu* MenuWithKbd(Str label, Str kbd);
    PopupMenu* Link(Str label, Str href, IconName icon = IconName::None);
    PopupMenu* Separator();
    PopupMenu* Label(Str label);
    PopupMenu* Element(El* el);
    PopupMenu* Submenu(Str label, PopupMenu* menu);
    // Applies to the last row added.
    PopupMenu* Disabled(bool v);
    PopupMenu* Checked(bool v);
    PopupMenu* Icon(IconName v);
    PopupMenu* Kbd(Str v);
    PopupMenu* WithSize(UiSize s);
    PopupMenu* MinW(float v);
    PopupMenu* MaxH(float v);
    PopupMenu* Scrollable(bool v = true);
    PopupMenu* CheckSide(Side s);
    PopupMenu* ExternalLinkIcon(bool v);
    El* IntoEl();
};

// The same keyed state, for a page that has to drive the menu itself — the
// key handler that walks it, or the trigger that opens it.
Entity<PopupMenuState> PopupMenuStateFor(Ctx* cx, Str id);

// dropdown_menu.rs: a trigger with a PopupMenu hanging off it. Rust hangs the
// menu in a Popover anchored to the trigger's corner; here the menu is a
// deferred child of the wrapper, which is the same layering.
struct DropdownMenu {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    El* trigger = nullptr;
    PopupMenu* menu = nullptr;
    // Anchor::TopRight rather than TopLeft: the menu's right edge lines up
    // with the trigger's.
    bool anchorRight = false;
    float gap = 4;

    static DropdownMenu* New(Ctx* cx, Str id);
    DropdownMenu* Trigger(El* e);
    DropdownMenu* Menu(PopupMenu* m);
    DropdownMenu* AnchorRight(bool v = true);
    El* IntoEl();
};

// context_menu.rs: an element whose right press opens a menu where the
// pointer is.
struct ContextMenu {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    El* child = nullptr;
    PopupMenu* menu = nullptr;

    static ContextMenu* New(Ctx* cx, Str id);
    ContextMenu* Child(El* e);
    ContextMenu* Menu(PopupMenu* m);
    El* IntoEl();
};

// app_menu_bar.rs: the row of menus across the top of a window. One is open at
// a time; the arrows walk them and Escape closes.
struct AppMenuBarState {
    int selected = -1;

    static void OnMenuClick(AppMenuBarState* self, Ctx* cx,
                            const ClickEvent* ev, intptr_t ix);
    // Once one menu is open, moving over another switches to it, which is
    // what a menu bar does everywhere.
    static void OnMenuHover(AppMenuBarState* self, Ctx* cx,
                            const HoverEvent* ev, intptr_t ix);
};

// on_move_left / on_move_right: both wrap, and neither does anything while no
// menu is open.
int AppMenuBarNextIndex(int selected, int count);
int AppMenuBarPrevIndex(int selected, int count);
// Escape, and the click that opens or closes one.
void AppMenuBarSelect(AppMenuBarState* s, Ctx* cx, int ix);

struct AppMenuBar {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<AppMenuBarState> state = {};
    Str titles[12] = {};
    PopupMenu* menus[12] = {};
    int n = 0;

    static AppMenuBar* New(Ctx* cx, Str id, Entity<AppMenuBarState> state);
    AppMenuBar* Menu(Str title, PopupMenu* menu);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
