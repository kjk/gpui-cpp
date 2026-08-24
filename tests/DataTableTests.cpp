/* Ported from crates/ui/src/table/state.rs.
 *
 * data_table.rs binds escape, the four arrows, home, end, pageup, pagedown
 * and tab in the table's key context. Only the four arrows consult
 * loop_selection; the page moves and Home/End clamp, and Home and End are the
 * first and last column rather than the first and last row. perform_sort
 * cycles one column at a time and resets whichever one was sorted before. */

#include "Test.h"

// The chord, resolved in the table's context, read as what the table does.
static TableAction ForChord(const char* spec) {
    TableInitKeys();
    KeyChord c = {};
    utassert(KeyChordParse(Str(spec), &c));
    uint32_t ctx = KeyContextOf(TableContext());
    return TableActionOf(KeymapMatch(c, &ctx, 1).action);
}

static void TheKeyTable() {
    utassert(ForChord("up") == TableAction::SelectPrev);
    utassert(ForChord("down") == TableAction::SelectNext);
    utassert(ForChord("left") == TableAction::SelectPrevColumn);
    utassert(ForChord("right") == TableAction::SelectNextColumn);
    // Tab is bound to the same action as Right, and shift-tab to Left — in
    // the table's own context, so the window's focus ring only ever sees a
    // tab the table did not want.
    utassert(ForChord("tab") == TableAction::SelectNextColumn);
    utassert(ForChord("shift-tab") == TableAction::SelectPrevColumn);
    utassert(ForChord("home") == TableAction::SelectFirst);
    utassert(ForChord("end") == TableAction::SelectLast);
    utassert(ForChord("pageup") == TableAction::SelectPageUp);
    utassert(ForChord("pagedown") == TableAction::SelectPageDown);
    utassert(ForChord("escape") == TableAction::Cancel);
    utassert(ForChord("space") == TableAction::None);
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
    // A column the table has never been asked about answers with what the
    // caller declared rather than growing the array to reach it.
    utassert(TableColWidth(&s, 40, 120) == 120);
    utassert(s.colWidth.len == 1);
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

// Four columns of a hundred, side by side.
static void SeedCols(TableState* s, Bounds* b, int n) {
    s->colCount = n;
    TableSeedColOrder(s, n);
    for (int i = 0; i < n; i++) {
        b[i] = {(float)i * 100.f, 0, 100, 30};
    }
}

static void TheGapIsAfterTheLastCentreLeftOfThePointer() {
    TableState s;
    Bounds b[4];
    SeedCols(&s, b, 4);
    // Dragging column 0: the pointer over the first half of column 1 is
    // still the gap it came from, so nothing moves.
    utassert(TableDragGapAt(b, 4, 120, 0) == -1);
    // Past the centre of column 1 it is the gap after it.
    utassert(TableDragGapAt(b, 4, 160, 0) == 2);
    utassert(TableDragGapAt(b, 4, 260, 0) == 3);
    utassert(TableDragGapAt(b, 4, 380, 0) == 4);
    // Dragging column 2 back to the front.
    utassert(TableDragGapAt(b, 4, 10, 2) == 0);
    utassert(TableDragGapAt(b, 4, 120, 2) == 1);
    // The two gaps either side of the dragged column are both a no-op.
    utassert(TableDragGapAt(b, 4, 210, 2) == -1);
    utassert(TableDragGapAt(b, 4, 260, 2) == -1);
}

static void AMovedColumnLandsInTheGap() {
    TableState s;
    Bounds b[4];
    SeedCols(&s, b, 4);
    // The first column into the gap after the second: everything before it
    // shifts down one.
    utassert(TableMoveColumn(&s, 0, 2));
    utassert(TableColAt(&s, 0) == 1);
    utassert(TableColAt(&s, 1) == 0);
    utassert(TableColAt(&s, 2) == 2);
    utassert(TableDisplayOfCol(&s, 0) == 1);

    // And back the other way: the last column to the front.
    utassert(TableMoveColumn(&s, 3, 0));
    utassert(TableColAt(&s, 0) == 3);
    utassert(TableColAt(&s, 1) == 1);
    utassert(TableColAt(&s, 2) == 0);
    utassert(TableColAt(&s, 3) == 2);

    // The gaps either side of a column leave it where it is.
    utassert(!TableMoveColumn(&s, 1, 1));
    utassert(!TableMoveColumn(&s, 1, 2));
    utassert(TableColAt(&s, 1) == 1);
}

static void LoadMoreAsksNearTheEnd() {
    TableState s;
    s.rowCount = 1000;
    utassert(!TableShouldLoadMore(&s, 995));
    s.hasMore = true;
    utassert(!TableShouldLoadMore(&s, 500));
    utassert(TableShouldLoadMore(&s, 980));
    s.loading = true;
    utassert(!TableShouldLoadMore(&s, 980));
}

// A cell's listener carries the row and the column as one number, which is
// what every listener that has two of them does here.
static void ACellIsOneNumber() {
    utassert(TableCellRow(TableCellPack(0, 0)) == 0);
    utassert(TableCellCol(TableCellPack(0, 0)) == 0);
    utassert(TableCellRow(TableCellPack(4999, 44)) == 4999);
    utassert(TableCellCol(TableCellPack(4999, 44)) == 44);
    // A million rows and four thousand columns still come back whole — on a
    // 64-bit target. The word is an intptr_t, so a 32-bit one has twenty bits
    // left over the column and tops out at half a million rows.
    const int bigRow = sizeof(intptr_t) >= 8 ? 1000000 : 500000;
    utassert(TableCellRow(TableCellPack(bigRow, 4095)) == bigRow);
    utassert(TableCellCol(TableCellPack(bigRow, 4095)) == 4095);
}

// The two right-click marks are exclusive, and a selection made any other
// way clears both — state.rs emits RightClickedRow(None) beside SelectRow so
// a context menu hanging off the old row is told to go.
static void ARightClickMarksARowOrACellButNeverBoth() {
    TableState s;
    s.rowCount = 8;
    s.colCount = 3;
    s.rowSelectable = true;

    s.rightClickedRow = 4;
    s.rightClickedCellRow = -1;
    s.rightClickedCellCol = -1;
    utassert(s.rightClickedRow == 4);

    // A cell mark replaces the row one outright.
    s.rightClickedCellRow = 2;
    s.rightClickedCellCol = 1;
    s.rightClickedRow = -1;
    utassert(s.rightClickedRow == -1);
    utassert(s.rightClickedCellRow == 2 && s.rightClickedCellCol == 1);

    // And the row that a cell mark names is not the selected row: the two
    // live in different fields, so a table can paint both.
    utassert(s.selectedRow == -1);
}

// update_visible_range_if_need: the delegate is told only when the range
// actually moved, and never about a range of one — Rust skips that because
// its virtual list lays a single item out to measure with, and here it is the
// frame before the pane has been laid out at all.
static void TheDelegateHearsAboutTheRangeOnlyWhenItMoves() {
    TableState s;
    utassert(TableVisibleRowsChanged(&s, 0, 20));
    // The same range again says nothing.
    utassert(!TableVisibleRowsChanged(&s, 0, 20));
    utassert(TableVisibleRowsChanged(&s, 5, 25));
    utassert(s.visibleRange.rowFirst == 5 && s.visibleRange.rowEnd == 25);
    // A range of one is the measuring pass, and does not even overwrite what
    // was last reported.
    utassert(!TableVisibleRowsChanged(&s, 0, 1));
    utassert(!TableVisibleRowsChanged(&s, 0, 0));
    utassert(s.visibleRange.rowFirst == 5 && s.visibleRange.rowEnd == 25);

    // The two axes are independent.
    utassert(TableVisibleColsChanged(&s, 2, 9));
    utassert(!TableVisibleColsChanged(&s, 2, 9));
    utassert(s.visibleRange.rowFirst == 5);
    utassert(s.visibleRange.colFirst == 2 && s.visibleRange.colEnd == 9);
}

// Which columns overlap the scrolling pane. The pinned ones are never in it —
// they do not move under the offset, so the run is counted from the first one
// that does, and the range runs one past the edge the way virtual_list does.
static void TheVisibleColumnsAreTheOnesUnderTheOffset() {
    TableState s;
    s.colCount = 6;
    TableSeedColOrder(&s, 6);
    TableEnsureCols(&s, 6);
    for (int c = 0; c < 6; c++) {
        s.colWidth[c] = 100;
    }
    int first = -1, end = -1;

    // A pane that has not been laid out yet answers an empty range rather
    // than every column, which the range-of-one rule then swallows.
    TableVisibleCols(&s, &first, &end);
    utassert(first == 0 && end == 0);

    // Two and a half columns fit; the one after the edge is built too.
    s.bodyBounds.w = 250;
    TableVisibleCols(&s, &first, &end);
    utassert(first == 0 && end == 4);

    // Slid over by one and a half columns: the second is still half on
    // screen, so it is still the first one visible.
    s.scrollX = 150;
    TableVisibleCols(&s, &first, &end);
    utassert(first == 1 && end == 6);

    // Two pinned columns: the offset moves the rest, and the range counts
    // over those four rather than over all six.
    s.scrollX = 0;
    s.fixedCols = 2;
    TableVisibleCols(&s, &first, &end);
    utassert(first == 0 && end == 4);
}

// scroll_to_col, which is what set_selected_col and set_selected_cell go
// through. The offset is over the columns that move, so a pinned one asks
// for the start of them and nothing else.
static void ScrollingToAColumnBringsItIn() {
    TableState s;
    s.colCount = 6;
    TableSeedColOrder(&s, 6);
    TableEnsureCols(&s, 6);
    for (int c = 0; c < 6; c++) {
        s.colWidth[c] = 100;
    }
    s.bodyBounds.w = 250;

    // A column already on screen does not move the offset.
    TableScrollToCol(&s, 1, ScrollStrategy::Top);
    utassert(s.scrollX == 0);
    // One off the right end comes in from that side: its far edge lands on
    // the viewport's, which is 500 - 250.
    TableScrollToCol(&s, 4, ScrollStrategy::Top);
    utassert(s.scrollX == 250);
    // And one off the left comes back in from that side.
    TableScrollToCol(&s, 0, ScrollStrategy::Top);
    utassert(s.scrollX == 0);

    // Pinned columns are not in the window at all: asking for one asks for
    // the first that moves, which is Rust's saturating_sub.
    s.fixedCols = 2;
    s.scrollX = 200;
    TableScrollToCol(&s, 0, ScrollStrategy::Top);
    utassert(s.scrollX == 0);
    // The last column, with two pinned: four move, so the content is 400
    // wide and the offset can reach 150.
    TableScrollToCol(&s, 5, ScrollStrategy::Top);
    utassert(s.scrollX == 150);

    // A pane that has not been laid out yet has nothing to scroll against.
    s.bodyBounds.w = 0;
    s.scrollX = 42;
    TableScrollToCol(&s, 5, ScrollStrategy::Top);
    utassert(s.scrollX == 42);
}

// refresh / prepare_col_groups: the table drops what it worked out for
// itself, so the caller's declarations are taken again.
static void ARefreshGivesTheColumnsBackToTheCaller() {
    TableState s;
    s.colCount = 4;
    TableSeedColOrder(&s, 4);
    for (int c = 0; c < 4; c++) {
        TableSeedColWidth(&s, c, 100);
    }
    s.colWidth[1] = 180;
    utassert(TableMoveColumn(&s, 0, 3));
    utassert(TableColAt(&s, 0) == 1);

    TableRefreshCols(&s);
    // The widths are unseeded, so the next build takes what is declared.
    TableSeedColWidth(&s, 1, 100);
    utassert(s.colWidth[1] == 100);
    // And the order is the caller's again.
    TableSeedColOrder(&s, 4);
    utassert(TableColAt(&s, 0) == 0);
    utassert(TableColAt(&s, 3) == 3);
}

// on_cell_click's opening rule. A table with a row header column has
// somewhere else to pick a row, so a second click on a selected cell is only
// ever a cell click; a table without one escalates instead.
static void ASecondClickOnACellTakesTheRowWhenThereIsNoRowHeader() {
    TableState s;
    s.rowCount = 8;
    s.colCount = 3;
    s.cellSelectable = true;
    s.rowSelectable = true;
    s.rowHeader = false;
    s.mode = TableSelectionMode::Cell;
    s.selectedCellRow = 3;
    s.selectedCellCol = 1;

    utassert(TableEscalatesToRow(&s, 3, 1, false));
    // A double click is the cell's, whatever else is true.
    utassert(!TableEscalatesToRow(&s, 3, 1, true));
    // Another cell is a plain selection.
    utassert(!TableEscalatesToRow(&s, 3, 2, false));
    utassert(!TableEscalatesToRow(&s, 4, 1, false));
    // With the header column there, nothing escalates.
    s.rowHeader = true;
    utassert(!TableEscalatesToRow(&s, 3, 1, false));
    // And a table whose rows cannot be selected has nowhere to escalate to.
    s.rowHeader = false;
    s.rowSelectable = false;
    utassert(!TableEscalatesToRow(&s, 3, 1, false));
    // Nor does one whose selection is a row already.
    s.rowSelectable = true;
    s.mode = TableSelectionMode::Row;
    utassert(!TableEscalatesToRow(&s, 3, 1, false));
}

void TestDataTable() {
    TheDelegateHearsAboutTheRangeOnlyWhenItMoves();
    TheVisibleColumnsAreTheOnesUnderTheOffset();
    ScrollingToAColumnBringsItIn();
    ARefreshGivesTheColumnsBackToTheCaller();
    ASecondClickOnACellTakesTheRowWhenThereIsNoRowHeader();
    ARightClickMarksARowOrACellButNeverBoth();
    ACellIsOneNumber();
    AColumnKeepsItsWidthOnceItHasOne();
    AResizeIsClamped();
    TheGapIsAfterTheLastCentreLeftOfThePointer();
    AMovedColumnLandsInTheGap();
    LoadMoreAsksNearTheEnd();
    TheKeyTable();
    TheSortCycle();
    OnlyOneColumnCarriesTheSort();
}
