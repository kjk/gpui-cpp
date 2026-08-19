/* Unstyled switch — crates/base/src/switch.rs */

#include "gpui/gpui.h"

namespace gpui {

// Rust's `Switch::new(id).checked(..).disabled(..).on_change(..)`. The checked
// value is the caller's; an activation reports the next one through
// `onChange`, which a
// `void On(T*, Ctx*, const ClickEvent*, intptr_t next)` reads as a bool. A
// disabled switch keeps its element id but takes neither focus nor the click.
struct Switch {
    static El* New(Ctx* cx, Str id, bool checked = false, bool disabled = false,
                   Listener onChange = {});
};

// The track carries an id of its own — Rust builds it from `(id, "track")` —
// so it hovers as a part without shadowing the switch's own hit box.
struct SwitchTrack {
    static El* New(Ctx* cx, Str id);
};

struct SwitchThumb {
    static El* New(Ctx* cx);
};
} // namespace gpui
