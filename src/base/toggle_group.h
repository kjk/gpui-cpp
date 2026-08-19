/* Unstyled toggle group — crates/base/src/toggle_group.rs */

#include "gpui/gpui.h"

namespace gpui {

// A container for a set of toggles. Rust carries an `axis`, but only to state
// the group's orientation to assistive technology — it never lays the children
// out — so there is nothing here for it to set. The caller picks the
// direction, the way it does in Rust.
struct ToggleGroup {
    static El* New(Ctx* cx, Str id);
};
} // namespace gpui
