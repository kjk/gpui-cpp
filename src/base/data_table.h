/* Unstyled data table — crates/ui/src/table/state.rs + table/data_table.rs */

#include "gpui/gpui.h"

namespace gpui {

// SelectionMode: what the last selection picked out.
enum class TableSelectionMode : uint8_t {
    None,
    Row,
    Column,
    Cell
};

// ColumnSort. A column with no sort of its own is Default, which is also the
// state the cycle passes back through.
enum class ColumnSort : uint8_t {
    Default,
    Ascending,
    Descending
};

// What a keystroke asks a table to do. data_table.rs binds escape, the four
// arrows, home, end, pageup, pagedown and tab in the table's key context.
enum class TableAction : uint8_t {
    None,
    SelectPrev,
    SelectNext,
    SelectPrevColumn,
    SelectNextColumn,
    SelectFirst,
    SelectLast,
    SelectPageUp,
    SelectPageDown,
    Cancel
};

TableAction TableActionForKey(int key);

// TableEvent, what the table tells whoever is listening.
enum class TableEventKind : uint8_t {
    SelectRow,
    SelectCol,
    SelectCell,
    DoubleClickedRow,
    DoubleClickedCell,
    Sort,
    ColumnWidthsChanged,
    Cancel
};

struct TableEvent {
    TableEventKind kind = TableEventKind::SelectRow;
    int row = -1;
    int col = -1;
    // What the column was sorted into, for a Sort.
    ColumnSort sort = ColumnSort::Default;
    // ColumnWidthsChanged(Vec<Pixels>): every column's width, once the drag
    // that changed one of them has ended. The array is the table's own, so a
    // listener reads it and does not keep it.
    const float* widths = nullptr;
    int nWidths = 0;
};

// How many columns a table keeps a width for. Rust's col_groups is a Vec;
// past this the width a caller declared is the one a column keeps.
const int kMaxTableCols = 32;

// The name a column-resize drag goes by, which is the `ResizeColumn` payload
// type in Rust.
extern const Str kTableResizeDrag;

// The resize handle's width, straddling the column's right edge.
const float kTableResizeHandleW = 2;

// What a table is between frames: the selection, what is sorted by what, and
// the flags that say which of those a user is allowed to move. The rows and
// columns themselves stay with the delegate, which is the caller here.
struct TableState {
    int rowCount = 0;
    int colCount = 0;
    TableSelectionMode mode = TableSelectionMode::None;
    int selectedRow = -1;
    int selectedCol = -1;
    int selectedCellRow = -1;
    int selectedCellCol = -1;
    // right_clicked_row: the row under a secondary press, cleared by the next
    // selection.
    int rightClickedRow = -1;
    bool rowSelectable = true;
    bool colSelectable = true;
    bool cellSelectable = false;
    // row_header: whether the table has a column of its own for picking rows.
    // Without one, clicking an already-selected cell takes the whole row.
    bool rowHeader = false;
    // loop_selection: whether the ends of the table wrap.
    bool loopSelection = false;
    bool sortable = true;
    // Which column carries the sort, and which way. Rust hangs the sort off
    // each column and resets the others; one pair says the same thing, since
    // only one column is ever sorted.
    int sortCol = -1;
    ColumnSort sort = ColumnSort::Default;
    // How far PageUp and PageDown move.
    int pageRows = 10;
    // col_resizable: whether a column edge can be dragged at all. Whether a
    // particular one can is the column's own business, and shows up as the
    // table putting a handle on it or not — which is also how Rust's
    // render_resize_handle reads it.
    bool colResizable = true;
    // ColGroup::width, seeded from what the caller declared for a column and
    // the table's own from then on. A zero is a column that has not been
    // seeded yet.
    float colWidth[kMaxTableCols] = {};
    // Column::min_width / max_width. Rust hangs a pair off every column and
    // every column in the tree takes the default pair, so one says as much.
    // A zero ceiling is Rust's f32::MAX, which is no ceiling.
    float colMinWidth = 20;
    float colMaxWidth = 0;
    // resizing_col: which edge is being dragged right now, and the flag that
    // decides whether a release has anything to report.
    int resizingCol = -1;
    Listener onEvent = {};

    static void OnRowClick(TableState* self, Ctx* cx, const ClickEvent* ev,
                           intptr_t row);
    static void OnRowMouseDown(TableState* self, Ctx* cx,
                               const MouseDownEvent* ev, intptr_t row);
    static void OnHeadClick(TableState* self, Ctx* cx, const ClickEvent* ev,
                            intptr_t col);
    static void OnSortClick(TableState* self, Ctx* cx, const ClickEvent* ev,
                            intptr_t col);
    // Which column the drag moves is the payload's, the way Rust reads it out
    // of the ResizeColumn it matched on.
    static void OnResizeDrag(TableState* self, Ctx* cx,
                             const DragMoveEvent* ev);
    static void OnResizeEnd(TableState* self, Ctx* cx, const MouseUpEvent* ev);
};

// The width a column is being drawn at, which is the caller's until the table
// has one of its own.
float TableColWidth(const TableState* s, int col, float declared);

// The width a column keeps, seeded once from what the caller declared.
void TableSeedColWidth(TableState* s, int col, float declared);

// size.clamp(min_width, max_width), which is what decides how far a drag on a
// column edge is allowed to get.
float TableClampColWidth(const TableState* s, float width);

// resize_cols: the column takes the new width, clamped, and the table is only
// notified if that changed anything.
void TableResizeCol(TableState* s, Ctx* cx, int col, float width);

// perform_sort's cycle: Default becomes Descending, Descending becomes
// Ascending, and Ascending goes back to Default.
ColumnSort TableNextSort(ColumnSort s);

// The sort a column is showing, given which column carries it.
ColumnSort TableSortOf(const TableState* s, int col);

void TablePerformSort(TableState* s, Ctx* cx, int col);
void TableSetSelectedRow(TableState* s, Ctx* cx, int row);
void TableSetSelectedCol(TableState* s, Ctx* cx, int col);
void TableSetSelectedCell(TableState* s, Ctx* cx, int row, int col);
void TableClearSelection(TableState* s, Ctx* cx);

// The action, applied.
void TablePerform(TableState* s, Ctx* cx, TableAction act);

} // namespace gpui
