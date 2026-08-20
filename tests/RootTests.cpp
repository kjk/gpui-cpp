/* Ported from crates/ui/src/root.rs.
 *
 * Root's own rules about the layers over the page: `render_dialog_layer`
 * walks the open dialogs and lets the last one that wants an overlay show it,
 * so a stack of them tints the page once; `render_notification_layer` insets
 * the notifications by the room an open sheet takes on its own edge. */

#include "Test.h"

using namespace gpui::component;

static void TheLastDialogThatWantsAnOverlayShowsIt() {
    const bool three[] = {true, false, true};
    utassert(RootDialogOverlayIndex(three, 3) == 2);
    // Only the first asked, so the overlay is under the first.
    const bool first[] = {true, false, false};
    utassert(RootDialogOverlayIndex(first, 3) == 0);
    // None of them asked, and none is shown.
    const bool none[] = {false, false};
    utassert(RootDialogOverlayIndex(none, 2) == -1);
    utassert(RootDialogOverlayIndex(nullptr, 0) == -1);
}

static void AnOpenSheetPushesTheNotificationsIn() {
    Edges e = RootNotificationInsets(true, SheetPlacement::Right, 350);
    utassertnear(e.right, 350.f);
    utassertnear(e.left, 0.f);
    utassertnear(e.top, 0.f);
    utassertnear(e.bottom, 0.f);

    e = RootNotificationInsets(true, SheetPlacement::Left, 350);
    utassertnear(e.left, 350.f);
    e = RootNotificationInsets(true, SheetPlacement::Top, 200);
    utassertnear(e.top, 200.f);
    e = RootNotificationInsets(true, SheetPlacement::Bottom, 200);
    utassertnear(e.bottom, 200.f);

    // With no sheet open they fill the window.
    e = RootNotificationInsets(false, SheetPlacement::Right, 350);
    utassertnear(e.right, 0.f);
    utassertnear(e.Horizontal(), 0.f);
    utassertnear(e.Vertical(), 0.f);
}

void TestRoot() {
    TestSuite("root");
    TheLastDialogThatWantsAnOverlayShowsIt();
    AnOpenSheetPushesTheNotificationsIn();
}
