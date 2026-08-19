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

static void AColumnKeepsItsWidthOnceItHasOne() {
    TableState s;
    s.colCount = 3;
    // Until a drag has moved it, a column is as wide as the caller declared.
    utassert(TableColWidth(&s, 0, 120) == 120);
    TableSeedColWidth(&s, 0, 120);
    utassert(TableColWidth(&s, 0, 120) == 120);
    // Seeding again does not undo a width the table has since taken.
    s.colWidth[0] = 200;
    TableSeedColWidth(&s, 0, 120);
    utassert(TableColWidth(&s, 0, 120) == 200);
    // Past the columns a table keeps widths for, the caller's is all there is.
    utassert(TableColWidth(&s, kMaxTableCols, 120) == 120);
}

static void AResizeIsClamped() {
    TableState s;
    // Rust's defaults: 20px at the narrow end and no ceiling at all.
    utassert(TableClampColWidth(&s, 5) == 20);
    utassert(TableClampColWidth(&s, 4000) == 4000);
    s.colMaxWidth = 450;
    utassert(TableClampColWidth(&s, 4000) == 450);
    utassert(TableClampColWidth(&s, 100) == 100);
}

void TestDataTable() {
    AColumnKeepsItsWidthOnceItHasOne();
    AResizeIsClamped();
    TheKeyTable();
    TheSortCycle();
    OnlyOneColumnCarriesTheSort();
}
