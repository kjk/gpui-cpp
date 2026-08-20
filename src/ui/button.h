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
    ButtonVariant variant = ButtonVariant::Default;
    UiSize size = UiSize::Medium;
    bool outline = false;
    bool disabled = false;
    bool loading = false;
    bool compact = false;
    bool selected = false;
    bool dropdown = false;
    bool focusRing = true;
    bool hasCustom = false;
    Rgba custom = {};
    Str tooltip = {};
    El* extra = nullptr;
    Listener onClick;
    // ButtonStyles: what the caller wants a selected or a disabled button to
    // look like, over what the variant computed. resolve_style's order is
    // fixed — the value state first, disabled last.
    StateStyle selectedStyle = {};
    StateStyle disabledStyle = {};

    static Button* New(Ctx* cx, Str id);
    Button* Label(Str s);
    Button* Icon(IconName n);
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
    Button* Selected(bool v);
    Button* SelectedStyle(const StateStyle& s);
    Button* DisabledStyle(const StateStyle& s);
    Button* DropdownCaret(bool v = true);
    Button* Custom(Rgba c);
    Button* Extra(El* e);
    Button* Loading(bool v);
    Button* Disabled(bool v);
    Button* WithSize(UiSize s);
    // FocusableExt::focus_ring: no focus appearance on this control.
    Button* FocusRing(bool v);
    Button* Tooltip(Str s);
    Button* OnClick(Listener l);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
