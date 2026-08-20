/* Themed checkbox — crates/ui/src/checkbox.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

struct Checkbox {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Str label = {};
    Str hint = {};
    // ParentElement: the element Rust's `.child(..)` puts under the label,
    // which the story fills with muted text or a markdown line.
    El* child = nullptr;
    bool checked = false;
    bool disabled = false;
    UiSize size = UiSize::Medium;
    Str tooltip = {};
    bool focusRing = true;
    float w = 0;
    Listener onClick;

    static Checkbox* New(Ctx* cx, Str id);
    Checkbox* Label(Str s);
    Checkbox* Hint(Str s);
    Checkbox* Child(El* e);
    Checkbox* Checked(bool v);
    Checkbox* Disabled(bool v);
    Checkbox* WithSize(UiSize s);
    Checkbox* W(float v);
    // FocusableExt::focus_ring: no focus appearance on this control.
    Checkbox* FocusRing(bool v);
    Checkbox* Tooltip(Str s);
    Checkbox* OnClick(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
