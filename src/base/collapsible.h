#ifndef GPUI_BASE_COLLAPSIBLE_H_
#define GPUI_BASE_COLLAPSIBLE_H_
/* Unstyled collapsible — crates/base/src/collapsible.rs */

#include "base/motion.h"

namespace gpui {

// Rust's Collapsible keeps its children in call order and drops the ones
// marked as content while it is closed — that filter is the whole module. Its
// base is a plain `div()` with no direction of its own, so the caller lays it
// out; this does the same, and the two callers that wanted a column say so.
struct Collapsible {
    El* root = nullptr;
    Ctx* cx = nullptr;
    bool open = false;
    // `reveal(id, progress)`: the content stays mounted and is revealed at
    // that normalized progress instead of being dropped while closed, which
    // is what lets a collapse animate rather than vanish.
    Str revealId = {};
    float revealProgress = 0;
    bool hasReveal = false;

    static Collapsible* New(Ctx* cx);
    // Rust's Collapsible impls Styled, so a caller sets its direction on the
    // collapsible itself. This is the one style it actually needs.
    Collapsible* FlexCol();
    Collapsible* Open(bool v);
    Collapsible* Reveal(Str id, float progress);
    Collapsible* Child(El* e);
    Collapsible* Content(El* e);
    El* IntoEl();
};
} // namespace gpui
#endif // GPUI_BASE_COLLAPSIBLE_H_
