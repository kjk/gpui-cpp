#ifndef GPUI_BASE_TOGGLE_GROUP_H_
#define GPUI_BASE_TOGGLE_GROUP_H_
/* Unstyled toggle group — crates/base/src/toggle_group.rs */

#include "gpui/gpui.h"

namespace gpui {

// A container for a set of toggles. Rust carries an `axis`, but only to state
// the group's orientation to assistive technology — it never lays the children
// out — so there is nothing here for it to set. The caller picks the
// direction, the way it does in Rust.
struct ToggleGroup {
    static El* New(Ctx* cx, Str id, Axis axis = Axis::Horizontal);
};
} // namespace gpui
#endif // GPUI_BASE_TOGGLE_GROUP_H_
