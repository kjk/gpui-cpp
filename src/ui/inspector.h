/* Themed inspector — crates/ui/src/inspector.rs

   GPUI's inspector picks an element out of the window and shows what it is,
   and then lets you edit its style live. Rust offers two editors over the
   same `StyleRefinement`, one spelling it as Rust source and one as JSON, both
   with LSP completions; there is no reflection table here and no language
   server, so what is ported is the JSON half over a subset written out by
   hand — `StyleField` in gpui.h — with the same shape around it: the editor,
   the parse error under it, and Reset. What is not ported is the Rust-source
   editor and the `#[track_caller]` source location Rust leads the panel
   with. */

#include "ui/sizing.h"

namespace gpui {

namespace component {

// The width Rust's inspector docks at.
const float kInspectorWidth = 320;

// The style of a picked element as the editor shows it, and the parse back.
// Only the fields `StyleField` names are written; `fields` says which of them
// the text actually named, so an override leaves the rest of the element's
// own style alone.
Str StyleToJson(Arena* a, const Style& style);
bool StyleFromJson(Arena* a, Str text, Style* style, uint32_t* fields,
                   Str* error);

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
