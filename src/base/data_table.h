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
    Cancel
};

struct TableEvent {
    TableEventKind kind = TableEventKind::SelectRow;
    int row = -1;
    int col = -1;
    // What the column was sorted into, for a Sort.
    ColumnSort sort = ColumnSort::Default;
};

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
    Listener onEvent = {};

    static void OnRowClick(TableState* self, Ctx* cx, const ClickEvent* ev,
                           intptr_t row);
    static void OnRowMouseDown(TableState* self, Ctx* cx,
                               const MouseDownEvent* ev, intptr_t row);
    static void OnHeadClick(TableState* self, Ctx* cx, const ClickEvent* ev,
                            intptr_t col);
    static void OnSortClick(TableState* self, Ctx* cx, const ClickEvent* ev,
                            intptr_t col);
};

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
