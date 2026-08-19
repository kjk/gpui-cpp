/* Ported from crates/base/src/dialog.rs.
 *
 * Rust's own cases there build a window; the two rules worth pinning are the
 * key bindings, which only exist while `keyboard` is on, and the four
 * conditions a backdrop press has to satisfy before it dismisses. */

#include "Test.h"

static void EscapeCancelsAndEnterConfirms() {
    utassert(DialogActionForKey(KeyEscape, true) == DialogAction::Cancel);
    utassert(DialogActionForKey(KeyReturn, true) == DialogAction::Confirm);
    utassert(DialogActionForKey(KeyTab, true) == DialogAction::None);
    utassert(DialogActionForKey(KeySpace, true) == DialogAction::None);
}

static void KeyboardOffRemovesTheBindings() {
    // Rust hangs the whole key context off `keyboard`, so neither binding
    // exists rather than each being checked and ignored.
    utassert(DialogActionForKey(KeyEscape, false) == DialogAction::None);
    utassert(DialogActionForKey(KeyReturn, false) == DialogAction::None);
}

static void ABackdropPressDismissesOnlyWhenAllFourHold() {
    // The ordinary case: left button, closable, topmost, below the band.
    utassert(DialogBackdropCloses(true, true, MouseButton::Left, 100, 34));
    // Above the reserved band, where a title bar still is.
    utassert(!DialogBackdropCloses(true, true, MouseButton::Left, 10, 34));
    // A secondary press is not a dismissal.
    utassert(!DialogBackdropCloses(true, true, MouseButton::Right, 100, 34));
    // overlay_closable off.
    utassert(!DialogBackdropCloses(false, true, MouseButton::Left, 100, 34));
    // Under another dialog, so the press belongs to the one on top.
    utassert(!DialogBackdropCloses(true, false, MouseButton::Left, 100, 34));
    // Exactly on the boundary counts as below it, as Rust's `<` says.
    utassert(DialogBackdropCloses(true, true, MouseButton::Left, 34, 34));
}

void TestDialog() {
    TestSuite("dialog");
    EscapeCancelsAndEnterConfirms();
    KeyboardOffRemovesTheBindings();
    ABackdropPressDismissesOnlyWhenAllFourHold();
}
