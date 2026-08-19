/* Ported from crates/ui/src/table/state.rs.
 *
 * data_table.rs binds escape, the four arrows, home, end, pageup, pagedown
 * and tab in the table's key context. Only the four arrows consult
 * loop_selection; the page moves and Home/End clamp, and Home and End are the
 * first and last column rather than the first and last row. perform_sort
 * cycles one column at a time and resets whichever one was sorted before. */

#include "Test.h"

static void TheKeyTable() {
    utassert(TableActionForKey(KeyUp) == TableAction::SelectPrev);
    utassert(TableActionForKey(KeyDown) == TableAction::SelectNext);
    utassert(TableActionForKey(KeyLeft) == TableAction::SelectPrevColumn);
    utassert(TableActionForKey(KeyRight) == TableAction::SelectNextColumn);
    // Tab is bound to the same action as Right.
    utassert(TableActionForKey(KeyTab) == TableAction::SelectNextColumn);
    utassert(TableActionForKey(KeyHome) == TableAction::SelectFirst);
    utassert(TableActionForKey(KeyEnd) == TableAction::SelectLast);
    utassert(TableActionForKey(KeyPageUp) == TableAction::SelectPageUp);
    utassert(TableActionForKey(KeyPageDown) == TableAction::SelectPageDown);
    utassert(TableActionForKey(KeyEscape) == TableAction::Cancel);
    utassert(TableActionForKey(KeySpace) == TableAction::None);
}

static void TheSortCycle() {
    utassert(TableNextSort(ColumnSort::Default) == ColumnSort::Descending);
    utassert(TableNextSort(ColumnSort::Descending) == ColumnSort::Ascending);
    utassert(TableNextSort(ColumnSort::Ascending) == ColumnSort::Default);
}

static void OnlyOneColumnCarriesTheSort() {
    TableState s;
    s.rowCount = 5;
    s.colCount = 3;
    utassert(TableSortOf(&s, 0) == ColumnSort::Default);

    s.sortCol = 1;
    s.sort = ColumnSort::Ascending;
    utassert(TableSortOf(&s, 1) == ColumnSort::Ascending);
    // Every other column reads as unsorted, which is what Rust's reset loop
    // leaves behind.
    utassert(TableSortOf(&s, 0) == ColumnSort::Default);
    utassert(TableSortOf(&s, 2) == ColumnSort::Default);
}

void TestDataTable() {
    TheKeyTable();
    TheSortCycle();
    OnlyOneColumnCarriesTheSort();
}
