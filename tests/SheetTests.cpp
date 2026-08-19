/* Ported from crates/base/src/sheet.rs, mod tests.
 *
 * Rust's four cases drive a window and click at a point; what they are
 * checking is the overlay's press rule, which is this. */

#include "Test.h"

static void ASheetOverlayPressHasThreeOutcomes() {
    // overlay_close_requests_then_notifies: a left press on a closable
    // overlay closes it.
    utassert(SheetOverlayPressAction(true, true, MouseButton::Left, 20, false,
                                     0) == SheetOverlayPress::Close);
    // non_closable_overlay_does_not_request_close — but it is still taken, so
    // the page behind never sees it.
    utassert(SheetOverlayPressAction(true, false, MouseButton::Left, 20, false,
                                     0) == SheetOverlayPress::Swallow);
    // An overlay that is not interactive is not there at all as far as the
    // pointer is concerned.
    utassert(SheetOverlayPressAction(false, true, MouseButton::Left, 20, false,
                                     0) == SheetOverlayPress::Ignore);
    // A secondary press is taken but does not close.
    utassert(SheetOverlayPressAction(true, true, MouseButton::Right, 20, false,
                                     0) == SheetOverlayPress::Swallow);
}

static void ThePressCutoffLeavesTheBandAboveAlone() {
    // pointer_above_the_dismiss_cutoff_is_ignored, with Rust's own numbers:
    // a cutoff of 50, a press at 20 ignored and one at 80 closing.
    utassert(SheetOverlayPressAction(true, true, MouseButton::Left, 20, true,
                                     50) == SheetOverlayPress::Ignore);
    utassert(SheetOverlayPressAction(true, true, MouseButton::Left, 80, true,
                                     50) == SheetOverlayPress::Close);
    // On the line counts as below it, as Rust's `<` says.
    utassert(SheetOverlayPressAction(true, true, MouseButton::Left, 50, true,
                                     50) == SheetOverlayPress::Close);
}

static void EscapeClosesASheet() {
    utassert(SheetClosesOnKey(KeyEscape));
    utassert(!SheetClosesOnKey(KeyReturn));
    utassert(!SheetClosesOnKey(KeyTab));
}

void TestSheet() {
    TestSuite("sheet");
    ASheetOverlayPressHasThreeOutcomes();
    ThePressCutoffLeavesTheBandAboveAlone();
    EscapeClosesASheet();
}
