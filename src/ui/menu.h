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

struct MenuItem {
    MenuItemKind kind = MenuItemKind::Item;
    Str label = {};
    IconName icon = IconName::None;
    // The keystroke shown on the right, which is Rust's Kbd of the item's
    // action.
    Str kbd = {};
    bool checked = false;
    bool disabled = false;
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
    MenuItem items[32] = {};
    int n = 0;
    UiSize size = UiSize::Medium;
    float minW = 180;
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
    PopupMenu* Separator();
    PopupMenu* Label(Str label);
    PopupMenu* Element(El* el);
    PopupMenu* Submenu(Str label, PopupMenu* menu);
    // Applies to the last row added.
    PopupMenu* Disabled(bool v);
    PopupMenu* WithSize(UiSize s);
    PopupMenu* MinW(float v);
    PopupMenu* CheckSide(Side s);
    El* IntoEl();

    // The two masks the key handling needs, over the rows as built.
    void Masks(bool* clickable, bool* hasSubmenu) const;
};

// The same keyed state, for a page that has to drive the menu itself — the
// key handler that walks it, or the trigger that opens it.
Entity<PopupMenuState> PopupMenuStateFor(Ctx* cx, Str id);

} // namespace component
} // namespace gpui
