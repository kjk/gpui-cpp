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
    // A million rows and four thousand columns still come back whole.
    utassert(TableCellRow(TableCellPack(1000000, 4095)) == 1000000);
    utassert(TableCellCol(TableCellPack(1000000, 4095)) == 4095);
}

void TestDataTable() {
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
