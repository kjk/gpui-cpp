/* Ported from crates/base/src/text_selection.rs.
 *
 * did_hit_text is the rule the module's mouse handling turns on, and
 * `blank_only_drag_never_publishes_or_copies_selection` is the case that pins
 * it. The rest of that module is participant registration and cross-view
 * projection, which the runtime here does with one document order over the
 * frame's text runs. */

#include "Test.h"

static void ADragThatNeverTouchesTextPublishesNothing() {
    TextSelectionGesture g;
    TextSelectionBegin(&g, false);
    TextSelectionExtend(&g, false);
    TextSelectionExtend(&g, false);
    TextSelectionEnd(&g);
    utassert(!TextSelectionPublishes(&g));
}

static void StartingInTheMarginAndDraggingOntoTextSelects() {
    // Rust takes the flag from `anchor.inside_text || endpoint.inside_text`,
    // so a press beside a paragraph still begins something.
    TextSelectionGesture g;
    TextSelectionBegin(&g, false);
    utassert(!TextSelectionPublishes(&g));
    TextSelectionExtend(&g, true);
    utassert(TextSelectionPublishes(&g));
}

static void OnceItHasTouchedTextItStays() {
    // |=, never cleared mid-gesture: dragging back off into the margin does
    // not throw away what was selected.
    TextSelectionGesture g;
    TextSelectionBegin(&g, true);
    TextSelectionExtend(&g, false);
    TextSelectionExtend(&g, false);
    utassert(TextSelectionPublishes(&g));
    // And it outlives the release, so it can still be copied.
    TextSelectionEnd(&g);
    utassert(!g.selecting);
    utassert(TextSelectionPublishes(&g));
}

static void AFreshGestureStartsOver() {
    TextSelectionGesture g;
    TextSelectionBegin(&g, true);
    TextSelectionEnd(&g);
    // Rust assigns rather than ORs on the press, so the last drag's hit does
    // not carry into this one.
    TextSelectionBegin(&g, false);
    utassert(!TextSelectionPublishes(&g));
}

static void ExtendingWithoutAGestureDoesNothing() {
    TextSelectionGesture g;
    // A move with no button down is not part of a selection.
    TextSelectionExtend(&g, true);
    utassert(!TextSelectionPublishes(&g));
}

static void ClearingDropsBoth() {
    TextSelectionGesture g;
    TextSelectionBegin(&g, true);
    TextSelectionClear(&g);
    utassert(!g.selecting);
    utassert(!TextSelectionPublishes(&g));
}

void TestTextSelection() {
    TestSuite("text_selection");
    ADragThatNeverTouchesTextPublishesNothing();
    StartingInTheMarginAndDraggingOntoTextSelects();
    OnceItHasTouchedTextItStays();
    AFreshGestureStartsOver();
    ExtendingWithoutAGestureDoesNothing();
    ClearingDropsBoth();
}
