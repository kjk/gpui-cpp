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

// ─── the window's selection ───────────────────────────────────────────────
//
// WindowSelectionState. Rust keeps one per window in a global keyed by
// WindowId: every selectable run registers with it as it paints, and the
// window's own mouse handlers — not the application's — drive the gesture,
// so a drag runs from a paragraph into the one below it without either of
// them knowing about the other. Here the registrations are the `TextHit`s
// the frame already collects, and the endpoints are offsets into that same
// document order, so this is the state and the handlers around it.
//
// The runtime calls the three gestures and TextSelectionApply; an
// application only has to say `Selectable()` on the text.
struct WindowSelection {
    TextSelectionGesture gesture;
    // SelectionEndpoint::anchor / cursor, as document offsets. -1 is none.
    int anchor = -1;
    int cursor = -1;
    // TextSelectionScopeId: the trap the gesture began in. Extending stays
    // inside it, and so does what gets painted and copied.
    int scope = 0;
};

// The window's selection, made on first use.
WindowSelection* WindowSelectionOf(Window* win);
void WindowSelectionFree(Window* win);

// The press. `clickCount` is GPUI's — 2 takes the word, 3 the line — and
// `extend` is a shift-click, which moves the cursor and keeps the anchor.
void WindowSelectionPress(Window* win, float x, float y, int clickCount,
                          bool extend);
// A move with the button down.
void WindowSelectionDrag(Window* win, float x, float y);
// The release. What was selected stands until the next press.
void WindowSelectionRelease(Window* win);

// TextSelection::has_selection.
bool WindowSelectionHas(const Window* win);
// TextSelection::clear.
void WindowSelectionClear(Window* win);
// TextSelection::selected_text, written into `out`. Answers its length.
int WindowSelectionText(Window* win, char* out, int cap);
// The window's copy: the selection to the clipboard. False when there is
// nothing selected, which is what lets a Ctrl+C fall through to whatever
// else wants it.
bool WindowSelectionCopy(Window* win);

// Hand the range to the frame being built, which is what makes it paint.
// The runtime calls this before the view renders.
void WindowSelectionApply(Window* win);
} // namespace gpui
