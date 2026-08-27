#ifndef GPUI_BASE_COLLAPSIBLE_H_
#define GPUI_BASE_COLLAPSIBLE_H_
/* Unstyled collapsible — crates/base/src/collapsible.rs */

#include "gpui/gpui.h"

namespace gpui {

// Rust's Collapsible keeps its children in call order and drops the ones
// marked as content while it is closed — that filter is the whole module. Its
// base is a plain `div()` with no direction of its own, so the caller lays it
// out; this does the same, and the two callers that wanted a column say so.
struct Collapsible {
    El* root = nullptr;
    bool open = false;

    static Collapsible* New(Ctx* cx);
    // Rust's Collapsible impls Styled, so a caller sets its direction on the
    // collapsible itself. This is the one style it actually needs.
    Collapsible* FlexCol();
    Collapsible* Open(bool v);
    Collapsible* Child(El* e);
    Collapsible* Content(El* e);
    El* IntoEl();
};
} // namespace gpui
#endif // GPUI_BASE_COLLAPSIBLE_H_
