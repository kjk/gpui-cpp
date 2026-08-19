/* Unstyled popup menu — crates/ui/src/menu/popup_menu.rs */

#include "gpui/gpui.h"

namespace gpui {

// What a keystroke asks an open menu to do. Rust binds enter, escape, up,
// down, left and right in the "PopupMenu" key context; this is that table,
// read as an answer rather than routed as an action.
enum class PopupMenuAction : uint8_t {
    None,
    SelectPrev,
    SelectNext,
    Confirm,
    Cancel,
    // The arrow that reaches into the selected item's submenu, and the one
    // that steps back out of it. Which arrow is which depends on the side the
    // submenus open towards, which is what Rust reads off submenu_anchor.
    OpenSubmenu,
    CloseSubmenu
};

// `side` is the side submenus open towards: Right is the usual left-to-right
// menu, where Right reaches in and Left steps out.
PopupMenuAction PopupMenuActionForKey(int key, Side side);

// select_down / select_up over the clickable items. A separator or a label is
// not clickable and is stepped over; both ends wrap. Rust's select_down with
// nothing selected takes index 0 whether or not that row is clickable, and
// select_up with nothing selected takes the last clickable row — this keeps
// both, quirk included.
int PopupMenuNextIndex(const bool* clickable, int n, int selected);
int PopupMenuPrevIndex(const bool* clickable, int n, int selected);

// What a menu is between frames. Rust keeps this in the PopupMenu entity
// along with its items; the items are the caller's here, so this is the part
// that answers keys and clicks.
struct PopupMenuState {
    bool open = false;
    // selected_index: none is -1.
    int selected = -1;
    // The item whose submenu is showing, or -1.
    int openSubmenu = -1;
    // Which side submenus open towards, which decides what Left and Right do.
    Side side = Side::Right;
    // What a confirmed item reports: the item's index, bound with
    // ListenerFill the way a component hands its caller the value it made.
    Listener onConfirm = {};

    static void OnItemClick(PopupMenuState* self, Ctx* cx, const ClickEvent* ev,
                            intptr_t ix);
    static void OnItemHover(PopupMenuState* self, Ctx* cx, const HoverEvent* ev,
                            intptr_t ix);
};

// Open and close, which are `PopupMenu::show` and `dismiss(&Cancel)`. A menu
// that closes forgets what was selected in it.
void PopupMenuOpen(PopupMenuState* s, Ctx* cx);
void PopupMenuDismiss(PopupMenuState* s, Ctx* cx);

// The action, applied. `clickable` is the mask over the items as the caller
// built them this frame; `hasSubmenu` says which of them open onto one.
void PopupMenuPerform(PopupMenuState* s, Ctx* cx, PopupMenuAction act,
                      const bool* clickable, const bool* hasSubmenu, int n);

// confirm: run the item and dismiss the menu, which is what Rust does for
// both a keyboard Enter and a click.
void PopupMenuConfirm(PopupMenuState* s, Ctx* cx, int ix);

} // namespace gpui
