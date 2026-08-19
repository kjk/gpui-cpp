/* Unstyled selectable text host — crates/base/src/text_selection.rs */

#include "gpui/gpui.h"

namespace gpui {

// Rust's did_hit_text, which is the rule the whole module's mouse handling
// turns on: a gesture that never touched a glyph publishes nothing and copies
// nothing, however far it dragged. `blank_only_drag_never_publishes_or_copies_
// selection` is the case that pins it.
//
// It is sticky in both directions, and that is the point. A press in the
// margin still begins a gesture, so dragging from beside a paragraph into it
// selects — Rust sets the flag from `anchor.inside_text || endpoint.inside_
// text` and then ORs every move into it. And once any point has landed on
// text the selection stands even as the pointer wanders back off, because the
// flag is never cleared mid-gesture.
struct TextSelectionGesture {
    bool selecting = false;
    bool didHitText = false;
};

// The press. `insideText` is whether it landed on a glyph rather than in the
// space around one.
void TextSelectionBegin(TextSelectionGesture* g, bool insideText);
// A move during the drag.
void TextSelectionExtend(TextSelectionGesture* g, bool insideText);
// The release. The flag outlives the gesture, so what was selected can still
// be copied afterwards.
void TextSelectionEnd(TextSelectionGesture* g);
// Whether there is a selection to show or copy at all.
bool TextSelectionPublishes(const TextSelectionGesture* g);
// A press that starts something else — a click on a control, or one that
// dismissed an overlay — drops the gesture and what it had.
void TextSelectionClear(TextSelectionGesture* g);

struct TextSelection {
    static El* New(Ctx* cx, Str id, int clickId = 0);
};
} // namespace gpui
