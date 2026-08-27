#ifndef GPUI_SRC_UI_RADIO_H_
#define GPUI_SRC_UI_RADIO_H_
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
    int tabIndex = 0;
    bool tabStop = true;
    Listener onClick;

    static Radio* New(Ctx* cx, Str id);
    Radio* Label(Str s);
    Radio* Hint(Str s);
    Radio* Checked(bool v);
    Radio* Disabled(bool v);
    Radio* WithSize(UiSize s);
    // FocusableExt::focus_ring: no focus appearance on this control.
    Radio* FocusRing(bool v);
    Radio* TabIndex(int v);
    Radio* TabStop(bool v);
    Radio* OnClick(Listener fn);
    El* IntoEl();
};

// crates/ui/src/radio.rs RadioGroup: a set of radios laid out on one axis,
// of which one is selected. The click reports the index, which is what Rust's
// `on_click(&usize)` hands its caller.
struct RadioGroup {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    ArenaVec<Radio*> radios;
    bool horizontal = false;
    int selected = -1;
    bool disabled = false;
    UiSize size = UiSize::Medium;
    Listener onClick;

    static RadioGroup* Vertical(Ctx* cx, Str id);
    static RadioGroup* Horizontal(Ctx* cx, Str id);
    RadioGroup* Child(Radio* r);
    // `impl From<&str> for Radio`: a bare label is a radio of its own.
    RadioGroup* Child(Str label);
    RadioGroup* Selected(int ix);
    RadioGroup* Disabled(bool v);
    RadioGroup* WithSize(UiSize s);
    RadioGroup* OnClick(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
#endif // GPUI_SRC_UI_RADIO_H_
