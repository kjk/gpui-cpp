/* Unstyled link — crates/base/src/link.rs */

#include "gpui/gpui.h"

namespace gpui {

// Rust's `Link::new(id).disabled(..).on_activate(..)`. A link owns identity,
// focus and activation and nothing else: `href` is target data and base never
// opens a URL itself, so navigation is the application's.
//
// Rust splits the activation in two — `open_with(href, event)`, the strategy
// the application injects, and then `on_activate(event)`. Here it is one
// handler: a Listener carries an intptr_t, not a string, and a frame-arena
// href would not outlive the frame that built it, so the href stays with the
// caller that already knows it. `activates` is then simply whether a handler
// was given, which is what Rust's own condition reduces to without one.
struct Link {
    static El* New(Ctx* cx, Str id, bool disabled = false,
                   Listener onActivate = {});
};
} // namespace gpui
