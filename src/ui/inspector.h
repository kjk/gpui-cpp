/* Themed inspector — crates/ui/src/inspector.rs

   GPUI's inspector picks an element out of the window and shows what it is;
   Rust then lets you edit its style live as Rust or JSON through an editor
   with LSP completions, which needs `#[track_caller]` source locations and a
   style reflection table this tree does not have. What is here is the panel
   and the picking: the magnifier, the highlight, and what the element it
   found can say for itself. */

#include "ui/sizing.h"

namespace gpui {

namespace component {

// The width Rust's inspector docks at.
const float kInspectorWidth = 320;

struct Inspector {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    float width = kInspectorWidth;

    static Inspector* New(Ctx* cx);
    Inspector* W(float v);
    // Null when the inspector is off, so a caller can hand the answer
    // straight to Child().
    El* IntoEl();
};

} // namespace component
} // namespace gpui
