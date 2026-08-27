/* Themed button — crates/ui/src/button/button.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

enum class ButtonVariant : uint8_t {
    Default,
    Primary,
    Secondary,
    Danger,
    Info,
    Success,
    Warning,
    Ghost,
    Link,
    Text
};

struct Button {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Str label = {};
    IconName icon = IconName::None;
    // ButtonIcon carries an `Icon`, and an Icon may name its own colour:
    // `.icon(Icon::new(x).text_color(c))`. Its size is not the caller's —
    // `with_size(icon_size)` overwrites that with the button's own.
    bool hasIconColor = false;
    Rgba iconColor = {};
    // An icon after the label rather than before it, which is what a row
    // laid out `flex_row_reverse` comes to — Pagination's Next button.
    IconName iconRight = IconName::None;
    ButtonVariant variant = ButtonVariant::Default;
    UiSize size = UiSize::Medium;
    bool outline = false;
    bool disabled = false;
    bool loading = false;
    bool compact = false;
    bool justifyStart = false;
    bool selected = false;
    bool dropdown = false;
    bool focusRing = true;
    int tabIndex = 0;
    bool tabStop = true;
    bool hasCustom = false;
    Rgba custom = {};
    Str tooltip = {};
    Str accessibilityId = {};
    AccessibilityRole accessibilityRole = AccessibilityRole::None;
    bool hasAccessibilityRole = false;
    bool accessibilityToggled = false;
    bool hasAccessibilityToggled = false;
    El* extra = nullptr;
    // Size::Size(px), when the caller gave one instead of a Size.
    float sizePx = 0;
    IconName loadingIcon = IconName::Loader;
    // ButtonGroup joins its children: the edges each one draws and whether it
    // keeps the group's rounding. Nothing else sets these.
    bool joined = false;
    bool edgeT = true, edgeB = true, edgeL = true, edgeR = true;
    Listener onClick;
    uint32_t clickAction = 0;
    intptr_t clickActionArg = 0;
    // ButtonStyles: what the caller wants a selected or a disabled button to
    // look like, over what the variant computed. resolve_style's order is
    // fixed — the value state first, disabled last.
    StateStyle selectedStyle = {};
    StateStyle disabledStyle = {};

    static Button* New(Ctx* cx, Str id);
    Button* Label(Str s);
    Button* Icon(IconName n);
    Button* IconColor(Rgba c);
    Button* IconRight(IconName n);
    Button* Primary();
    Button* Secondary();
    Button* Danger();
    Button* Warning();
    Button* Success();
    Button* Info();
    Button* Ghost();
    Button* Link();
    Button* Text();
    Button* Outline();
    Button* Compact();
    // `.justify_start()`: a full-width button whose content sits at its
    // leading edge rather than in the middle.
    Button* JustifyStart(bool v = true);
    Button* Selected(bool v);
    Button* SelectedStyle(const StateStyle& s);
    Button* DisabledStyle(const StateStyle& s);
    Button* DropdownCaret(bool v = true);
    Button* Custom(Rgba c);
    Button* Extra(El* e);
    Button* Loading(bool v);
    Button* Disabled(bool v);
    Button* WithSize(UiSize s);
    // Sizable's Size::Size(px): a square button of exactly this many pixels,
    // which is what the Custom size section asks for.
    Button* Size(float px);
    // ButtonIcon::loading_icon: what spins in place of `icon` while loading.
    Button* LoadingIcon(IconName n);
    // FocusableExt::focus_ring: no focus appearance on this control.
    // FocusHandle::tab_index / tab_stop: where this control sits in the
    // Tab order, and whether Tab stops on it at all.
    Button* TabIndex(int v);
    Button* TabStop(bool v);
    Button* FocusRing(bool v);
    Button* Tooltip(Str s);
    Button* AccessibilityId(Str s);
    Button* Role(AccessibilityRole role);
    // Accessibility state only; Selected controls the visual state.
    Button* Toggled(bool v = true);
    Button* OnClick(Listener l);
    Button* OnClickAction(uint32_t action, intptr_t arg = 0);
    El* IntoEl();
};

// crates/ui/src/button/dropdown_button.rs: a split button — an action button
// joined to a caret-only button that opens a menu. Two buttons, not one with a
// caret. The halves stay joined except for a ghost split that is not selected,
// which reads better in a toolbar as two separate buttons.
struct DropdownMenu;
struct PopupMenu;

struct DropdownButton {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Button* button = nullptr;
    PopupMenu* menu = nullptr;
    bool selected = false;
    bool disabled = false;
    bool outline = false;
    // The props applied to both halves. Unset means the inner Button keeps
    // whatever it was given, and its value is what the caret takes too.
    // Action-specific props -- compact, loading, tooltip, the click handler --
    // belong to the inner Button and are not mirrored onto the caret.
    bool hasVariant = false;
    ButtonVariant variant = ButtonVariant::Default;
    bool hasSize = false;
    UiSize size = UiSize::Medium;
    // Anchor::TopRight by default; the story's first one asks for
    // BottomRight, which lines the same edge up.
    bool anchorRight = true;

    static DropdownButton* New(Ctx* cx, Str id);
    DropdownButton* Button_(component::Button* b);
    DropdownButton* Menu(PopupMenu* m);
    DropdownButton* Selected(bool v);
    DropdownButton* Disabled(bool v);
    DropdownButton* Outline();
    DropdownButton* WithVariant(ButtonVariant v);
    DropdownButton* WithSize(UiSize s);
    El* IntoEl();
};

// crates/ui/src/button/button_group.rs: buttons joined into one control,
// which is also a toggle group. The selection slice belongs to the dispatch
// and is valid for the duration of the listener call, like Rust's &Vec<usize>.
struct ButtonGroupEvent {
    const int* selected = nullptr;
    int count = 0;

    bool Contains(int index) const {
        for (int i = 0; i < count; i++) {
            if (selected[i] == index) {
                return true;
            }
        }
        return false;
    }
};

struct ButtonGroup {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    ArenaVec<Button*> children;
    bool multiple = false;
    bool disabled = false;
    bool vertical = false;
    bool compact = false;
    bool outline = false;
    bool hasVariant = false;
    ButtonVariant variant = ButtonVariant::Default;
    bool hasSize = false;
    UiSize size = UiSize::Medium;
    // Receives ButtonGroupEvent, the ordered indices selected after the click.
    Listener onClick;

    static ButtonGroup* New(Ctx* cx, Str id);
    ButtonGroup* Child(Button* b);
    ButtonGroup* Multiple(bool v);
    ButtonGroup* Disabled(bool v);
    ButtonGroup* Vertical(bool v = true);
    ButtonGroup* Compact();
    ButtonGroup* Outline();
    ButtonGroup* WithVariant(ButtonVariant v);
    ButtonGroup* WithSize(UiSize s);
    ButtonGroup* OnClick(Listener l);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
