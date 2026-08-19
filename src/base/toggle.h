/* Unstyled toggle — crates/base/src/toggle.rs */

#include "gpui/gpui.h"

namespace gpui {

// Rust's `Toggle::new(id).pressed(..).disabled(..).on_change(..)`: a
// controlled toggle button that owns identity, focus and activation and
// leaves every pixel to the caller. `onChange` is handed the value the
// activation produces — `!pressed` — which a
// `void On(T*, Ctx*, const ClickEvent*, intptr_t next)` reads as a bool. A
// disabled toggle keeps its element id but takes neither focus nor the click.
struct Toggle {
    static El* New(Ctx* cx, Str id, bool pressed = false, bool disabled = false,
                   Listener onChange = {});
};
} // namespace gpui
