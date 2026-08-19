/* Unstyled radio group — crates/base/src/radio_group.rs */

#include "gpui/gpui.h"

namespace gpui {

// A container for a set of radios. As with ToggleGroup, Rust's `axis` states
// the group's orientation to assistive technology and lays nothing out, so the
// caller picks the direction here too.
struct RadioGroup {
    static El* New(Ctx* cx, Str id);
};
} // namespace gpui
