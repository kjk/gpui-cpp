/* Unstyled radio — crates/base/src/radio.rs */

#include "gpui/gpui.h"

namespace gpui {

// Rust's `Radio::new(id).checked(..).disabled(..).on_change(..)`. Selection is
// the caller's; activating an unchecked radio asks for `true` through
// `onChange`. Picking the one that is already picked is not an activation, so
// a checked radio takes focus but no click — which is what the accessibility
// test pins by refusing it the Click action. A disabled radio keeps its
// element id and takes neither.
struct Radio {
    static El* New(Ctx* cx, Str id, bool checked = false, bool disabled = false,
                   Listener onChange = {});
};
} // namespace gpui
