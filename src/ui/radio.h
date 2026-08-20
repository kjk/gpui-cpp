/* Themed radio — crates/ui/src/radio.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

struct Radio {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Str label = {};
    Str hint = {};
    bool checked = false;
    bool disabled = false;
    UiSize size = UiSize::Medium;
    bool focusRing = true;
    Listener onClick;

    static Radio* New(Ctx* cx, Str id);
    Radio* Label(Str s);
    Radio* Hint(Str s);
    Radio* Checked(bool v);
    Radio* Disabled(bool v);
    Radio* WithSize(UiSize s);
    // FocusableExt::focus_ring: no focus appearance on this control.
    Radio* FocusRing(bool v);
    Radio* OnClick(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
