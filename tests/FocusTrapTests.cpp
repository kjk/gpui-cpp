/* Ported from crates/base/src/focus_trap.rs and the Tab actions that read it
 * in crates/ui/src/root.rs, plus FocusHandle::tab_index / tab_stop.
 *
 * A trap is a container that Tab cannot leave. The rules are: focus inside a
 * trap cycles within it, focus outside every trap never wanders into one, and
 * a container that has just opened takes focus into itself. On top of that,
 * the traversal visits the lowest tab index first and skips anything that is
 * focusable without being a stop. */

#include "Test.h"

// A window with focusables in tree order, as a frame's FocusCollect leaves
// them. `traps[i]` is the trap element i sits inside, 0 for none.
static Window* WindowWithFocusables(const int* ids, const int* traps, int n) {
    Window* win = new Window();
    for (int i = 0; i < n; i++) {
        FocusRect fr;
        fr.id = ids[i];
        fr.trapId = traps[i];
        win->focusEls.Append(fr);
    }
    return win;
}

static void TabInsideATrapCyclesWithinIt() {
    // Two outside, then three in a trap.
    int ids[] = {1, 2, 11, 12, 13};
    int traps[] = {0, 0, 7, 7, 7};
    Window* win = WindowWithFocusables(ids, traps, 5);

    win->focusId = 11;
    utassert(FocusTrapActive(win) == 7);
    utassert(FocusTrapTab(win, false) == 12);
    utassert(FocusTrapTab(win, false) == 13);
    // The end of the trap comes back to its start rather than to the page.
    utassert(FocusTrapTab(win, false) == 11);
    // And backwards off the front goes to the back of the same trap.
    utassert(FocusTrapTab(win, true) == 13);
    delete win;
}

static void TabOutsideEveryTrapNeverWandersIn() {
    int ids[] = {1, 2, 11, 12, 3};
    int traps[] = {0, 0, 7, 7, 0};
    Window* win = WindowWithFocusables(ids, traps, 5);

    win->focusId = 1;
    utassert(FocusTrapActive(win) == 0);
    utassert(FocusTrapTab(win, false) == 2);
    // 11 and 12 are behind the trap, so the page's own cycle skips them.
    utassert(FocusTrapTab(win, false) == 3);
    utassert(FocusTrapTab(win, false) == 1);
    delete win;
}

static void TwoTrapsDoNotReachEachOther() {
    int ids[] = {11, 12, 21, 22};
    int traps[] = {7, 7, 8, 8};
    Window* win = WindowWithFocusables(ids, traps, 4);

    win->focusId = 12;
    utassert(FocusTrapTab(win, false) == 11);
    win->focusId = 21;
    utassert(FocusTrapTab(win, false) == 22);
    utassert(FocusTrapTab(win, false) == 21);
    delete win;
}

// The dialog that has just opened: Rust tracks focus on the trap container, so
// the container holds focus from its first frame.
static void AnOpenContainerTakesFocusIntoItself() {
    int ids[] = {1, 11, 12};
    int traps[] = {0, 7, 7};
    Window* win = WindowWithFocusables(ids, traps, 3);

    win->focusId = 1;
    FocusTrapArm(win, 7);
    FocusTrapApplyPending(win);
    utassert(win->focusId == 11);
    utassert(FocusTrapActive(win) == 7);

    // Once inside, the next frame leaves focus where the reader put it.
    win->focusId = 12;
    FocusTrapArm(win, 7);
    FocusTrapApplyPending(win);
    utassert(win->focusId == 12);
    delete win;
}

static void ATrapWithNothingFocusableLeavesFocusAlone() {
    int ids[] = {1, 2};
    int traps[] = {0, 0};
    Window* win = WindowWithFocusables(ids, traps, 2);

    win->focusId = 2;
    utassert(!FocusTrapEnter(win, 7));
    FocusTrapArm(win, 7);
    FocusTrapApplyPending(win);
    utassert(win->focusId == 2);
    // Tab still moves, because there is no trap holding it.
    utassert(FocusTrapTab(win, false) == 1);
    delete win;
}

// Nothing armed is the ordinary frame: focus is not touched.
static void NothingArmedTouchesNothing() {
    int ids[] = {1, 11};
    int traps[] = {0, 7};
    Window* win = WindowWithFocusables(ids, traps, 2);

    win->focusId = 1;
    win->pendingTrap = 0;
    FocusTrapApplyPending(win);
    utassert(win->focusId == 1);
    delete win;
}

// A trap id is a name's hash, so the same container traps under the same id
// every frame, and two names do not collide.
static void ATrapIsNamedNotNumbered() {
    utassert(FocusTrapId(StrL("dialog")) == FocusTrapId(StrL("dialog")));
    utassert(FocusTrapId(StrL("dialog")) != FocusTrapId(StrL("sheet")));
    utassert(FocusTrapId(StrL("dialog")) != 0);
}

// FocusHandle::tab_stop(false): still focusable, still shows its ring when it
// is clicked, simply not somewhere Tab stops. An input's clear button and a
// dock tab bar's tools are what Rust turns it off for.
static void TabSkipsWhatIsNotAStop() {
    int ids[] = {1, 2, 3};
    int traps[] = {0, 0, 0};
    Window* win = WindowWithFocusables(ids, traps, 3);
    win->focusEls[1].tabStop = false;

    win->focusId = 1;
    utassert(FocusTrapTab(win, false) == 3);
    utassert(FocusTrapTab(win, true) == 1);
    delete win;
}

// A trap made only of non-stops leaves focus where it is rather than spinning.
static void ATrapOfNonStopsLeavesFocusAlone() {
    int ids[] = {1, 11, 12};
    int traps[] = {0, 7, 7};
    Window* win = WindowWithFocusables(ids, traps, 3);
    win->focusEls[1].tabStop = false;
    win->focusEls[2].tabStop = false;

    win->focusId = 11;
    utassert(FocusTrapTab(win, false) == 11);
    // Nor does arming the trap put focus on one of them.
    win->focusId = 1;
    utassert(!FocusTrapEnter(win, 7));
    utassert(win->focusId == 1);
    delete win;
}

// A dialog that is all text: the trap holds nothing that takes tab, so the
// container itself is what focus goes to — Rust tracks focus on the trap
// container, which is what leaves such a dialog able to hear escape.
static void ATrapWithNoStopFallsBackToItsOwnContainer() {
    int ids[] = {1, 7, 12};
    int traps[] = {0, 7, 7};
    Window* win = WindowWithFocusables(ids, traps, 3);
    // The container tracks focus without taking tab, and so does the one
    // control inside it.
    win->focusEls[1].tabStop = false;
    win->focusEls[2].tabStop = false;

    win->focusId = 1;
    FocusTrapArm(win, 7, 7);
    FocusTrapApplyPending(win);
    utassert(win->focusId == 7);

    // With a real stop in it, the stop wins and the container is skipped.
    win->focusEls[2].tabStop = true;
    win->focusId = 1;
    FocusTrapArm(win, 7, 7);
    FocusTrapApplyPending(win);
    utassert(win->focusId == 12);

    // A host that is not on screen leaves focus where it was.
    win->focusEls[2].tabStop = false;
    win->focusId = 1;
    FocusTrapArm(win, 7, 99);
    FocusTrapApplyPending(win);
    utassert(win->focusId == 1);
    delete win;
}

// tab_index: the traversal is by index first and by paint order inside it, so
// a control can be reached before one laid out above it. The sort is stable,
// which is what keeps everything at the default index in tree order.
static void TheTabIndexGroupsTheTraversal() {
    Arena* a = ArenaNew();
    Window* win = new Window();
    // Painted 1, 2, 3, 4 — but 3 asks to come first and 1 to come last.
    El* root = Div(a)
                   ->Child(Div(a)->FocusId(1)->TabIndex(2))
                   ->Child(Div(a)->FocusId(2))
                   ->Child(Div(a)->FocusId(3)->TabIndex(-1))
                   ->Child(Div(a)->FocusId(4));
    FocusCollect(win, root);
    utassert(win->focusEls.len == 4);
    utassert(win->focusEls[0].id == 3);
    utassert(win->focusEls[1].id == 2);
    utassert(win->focusEls[2].id == 4);
    utassert(win->focusEls[3].id == 1);

    win->focusId = 3;
    utassert(FocusTrapTab(win, false) == 2);
    utassert(FocusTrapTab(win, false) == 4);
    utassert(FocusTrapTab(win, false) == 1);
    utassert(FocusTrapTab(win, false) == 3);
    delete win;
    ArenaDelete(a);
}

void TestFocusTrap() {
    TestSuite("focus_trap");
    TabInsideATrapCyclesWithinIt();
    TabOutsideEveryTrapNeverWandersIn();
    TwoTrapsDoNotReachEachOther();
    AnOpenContainerTakesFocusIntoItself();
    ATrapWithNothingFocusableLeavesFocusAlone();
    NothingArmedTouchesNothing();
    ATrapIsNamedNotNumbered();
    TabSkipsWhatIsNotAStop();
    ATrapOfNonStopsLeavesFocusAlone();
    ATrapWithNoStopFallsBackToItsOwnContainer();
    TheTabIndexGroupsTheTraversal();
}
