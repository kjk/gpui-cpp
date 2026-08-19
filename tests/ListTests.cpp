/* Ported from crates/ui/src/list/list.rs and list/cache.rs.
 *
 * Rust binds up, down, enter and escape in the "List" key context and hangs
 * an on_action off each; ListActionForKey is that table. The moves themselves
 * are rows_cache.next / .prev, which wrap at both ends and start from the
 * first or the last row when nothing is selected. The row walk here is over a
 * flat count rather than an IndexPath through sections, so the section
 * headers a Rust cache steps over have nothing to step over here. */

#include "Test.h"

static void TheKeyTable() {
    utassert(ListActionForKey(KeyUp) == ListAction::SelectPrev);
    utassert(ListActionForKey(KeyDown) == ListAction::SelectNext);
    utassert(ListActionForKey(KeyReturn) == ListAction::Confirm);
    utassert(ListActionForKey(KeyEscape) == ListAction::Cancel);
    utassert(ListActionForKey(KeySpace) == ListAction::None);
    utassert(ListActionForKey(KeyTab) == ListAction::None);
}

static void NextAndPrevWrap() {
    ListState s;
    s.count = 3;

    // next(None) is the first row, prev(None) the last.
    utassert(ListNextIndex(&s) == 0);
    utassert(ListPrevIndex(&s) == 2);

    s.selected = 0;
    utassert(ListNextIndex(&s) == 1);
    utassert(ListPrevIndex(&s) == 2);

    s.selected = 2;
    utassert(ListNextIndex(&s) == 0);
    utassert(ListPrevIndex(&s) == 1);
}

static void AnEmptyListHasNowhereToGo() {
    ListState s;
    s.count = 0;
    utassert(ListNextIndex(&s) == -1);
    utassert(ListPrevIndex(&s) == -1);
}

void TestList() {
    TheKeyTable();
    NextAndPrevWrap();
    AnEmptyListHasNowhereToGo();
}
