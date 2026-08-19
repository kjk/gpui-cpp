/* Unstyled button — crates/base/src/button.rs */

#include "gpui/gpui.h"

namespace gpui {

// Rust's `Button::new(id).disabled(..).on_click(..).focusable(..)`. The button
// owns identity, focus and the click; layout, color and typography stay with
// the caller.
//
// `focusable` is Rust's own escape hatch, and its reasoning carries over
// exactly: a non-focusable button leaves focus where it already is, so a
// composed control does not flicker its ring on every press — and it gives up
// Enter and Space with it, so use it only where a focused sibling already
// exposes the action to the keyboard, the way a number input's step buttons
// do. A disabled button keeps its element id and takes neither focus nor the
// click.
struct Button {
    static El* New(Ctx* cx, Str id, bool disabled = false,
                   Listener onClick = {}, bool focusable = true);
};
} // namespace gpui
