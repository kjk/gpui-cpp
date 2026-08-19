/* Unstyled tabs — crates/base/src/tabs.rs */

#include "gpui/gpui.h"

namespace gpui {

// A collection root for tabs. Selection and activation live on the children,
// so the root is identity and nothing else.
struct Tabs {
    static El* New(Ctx* cx, Str id);
};

// Rust's `Tab::new(id).disabled(..).on_click(..)`. A tab does not take
// keyboard focus of its own — Rust says so outright, leaving that to a
// compound tab list that does not exist yet — so it gets identity and the
// click and no FocusId. `onClick` is passed through untouched: a tab produces
// no value of its own, so whichever index the caller bound is what its handler
// reads. Rust's `selected` only states the choice to assistive technology; the
// caller paints it, so there is nothing here for it to carry.
struct Tab {
    static El* New(Ctx* cx, Str id, bool disabled = false,
                   Listener onClick = {});
};
} // namespace gpui
