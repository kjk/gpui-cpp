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

// ─── the window's selection ───────────────────────────────────────────────
//
// WindowSelectionState over the frame's registered runs. A window is a plain
// struct, so a test can stand one up with hand-built TextHits — the
// registrations a real frame collects as it paints — and drive the same
// press / drag / release the runtime calls.

// Register a run: `y` is its row, and the text is one line 100 wide.
static void AddRun(Window* win, float y, const char* text, int scope) {
    TextHit h;
    h.bounds = {20, y, 100, 20};
    h.text = Str((char*)text);
    h.font = 14;
    h.maxW = 100;
    h.docOff = win->paint.textDocLen;
    h.scope = scope;
    win->paint.texts.Append(h);
    // The gap of one, which is where CopyTextHits puts the newline between
    // two runs.
    win->paint.textDocLen += h.text.len + 1;
}

static void AWindowWithNoTextSelectsNothing() {
    Window win;
    WindowSelectionPress(&win, 5, 5, 1, false);
    utassert(!WindowSelectionHas(&win));
    WindowSelectionFree(&win);
}

// A press that lands on no run at all drops what was selected: the outside
// click that clears a selection.
static void APressOffTextClearsIt() {
    Window win;
    AddRun(&win, 0, "hello", 0);
    AddRun(&win, 40, "world", 0);
    WindowSelectionPress(&win, 30, 5, 1, false);
    WindowSelectionDrag(&win, 30, 45);
    WindowSelectionRelease(&win);
    utassert(WindowSelectionHas(&win));
    // Far below both runs, and not nearest-clamped: nothing is there.
    WindowSelectionPress(&win, 500, 500, 1, false);
    utassert(!WindowSelectionHas(&win));
    WindowSelectionFree(&win);
}

// The whole point of a window-wide selection: a drag that starts in one run
// and ends in another covers both, with a newline where the runs meet.
static void ADragAcrossTwoRunsCopiesBoth() {
    Window win;
    AddRun(&win, 0, "hello", 0);
    AddRun(&win, 40, "world", 0);
    WindowSelectionPress(&win, 25, 5, 1, false);
    WindowSelectionDrag(&win, 115, 45);
    WindowSelectionRelease(&win);
    utassert(WindowSelectionHas(&win));
    char buf[64];
    int n = WindowSelectionText(&win, buf, (int)sizeof(buf));
    utassert(n > 0);
    // Without a text backend a hit resolves to the start of its run, so what
    // is pinned here is the span and the join, not the glyph the drag ended
    // on: the first run, the newline between them, and into the second.
    utassert(memcmp(buf, "hello\n", 6) == 0);
    WindowSelectionFree(&win);
}

// TextSelectionScopeId: a gesture that began inside a trap stays there, so a
// drag out of a dialog does not take the page behind it.
static void ADragOutOfAScopeStaysInIt() {
    Window win;
    const int kDialog = 7;
    AddRun(&win, 0, "page", 0);
    AddRun(&win, 40, "dialog", kDialog);
    WindowSelectionPress(&win, 25, 45, 1, false);
    utassert(win.sel->scope == kDialog);
    // Over the page's run, which is in another scope: the cursor does not
    // follow it there.
    WindowSelectionDrag(&win, 115, 5);
    WindowSelectionRelease(&win);
    char buf[64];
    int n = WindowSelectionText(&win, buf, (int)sizeof(buf));
    utassert(n == 0 || memcmp(buf, "page", 4) != 0);
    // And the frame is told which scope the range belongs to, so a run
    // outside it does not paint one.
    WindowSelectionApply(&win);
    utassert(win.paint.selScope == kDialog);
    WindowSelectionFree(&win);
}

// did_hit_text again, this time through the window: a drag that only ever
// touched the margin publishes nothing, so there is nothing to copy.
static void AMarginOnlyDragPublishesNothing() {
    Window win;
    AddRun(&win, 0, "hello", 0);
    // Well below the run: found only because the press clamps to the
    // nearest, never because it was on a glyph.
    WindowSelectionPress(&win, 25, 200, 1, false);
    WindowSelectionDrag(&win, 40, 220);
    WindowSelectionRelease(&win);
    utassert(!WindowSelectionHas(&win));
    char buf[16];
    utassert(WindowSelectionText(&win, buf, (int)sizeof(buf)) == 0);
    WindowSelectionApply(&win);
    utassert(win.paint.selA < 0);
    WindowSelectionFree(&win);
}

// A shift-click moves the cursor and keeps the anchor — Rust's
// begin_in_window(.., extend).
static void ShiftClickExtendsFromTheAnchor() {
    Window win;
    AddRun(&win, 0, "hello", 0);
    AddRun(&win, 40, "world", 0);
    WindowSelectionPress(&win, 25, 5, 1, false);
    WindowSelectionRelease(&win);
    int anchor = win.sel->anchor;
    WindowSelectionPress(&win, 25, 45, 1, true);
    utassert(win.sel->anchor == anchor);
    utassert(win.sel->cursor != anchor);
    WindowSelectionFree(&win);
}

void TestTextSelection() {
    TestSuite("text_selection");
    ADragThatNeverTouchesTextPublishesNothing();
    StartingInTheMarginAndDraggingOntoTextSelects();
    OnceItHasTouchedTextItStays();
    AFreshGestureStartsOver();
    ExtendingWithoutAGestureDoesNothing();
    ClearingDropsBoth();
    AWindowWithNoTextSelectsNothing();
    APressOffTextClearsIt();
    ADragAcrossTwoRunsCopiesBoth();
    ADragOutOfAScopeStaysInIt();
    AMarginOnlyDragPublishesNothing();
    ShiftClickExtendsFromTheAnchor();
}
