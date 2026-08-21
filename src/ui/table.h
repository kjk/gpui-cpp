/* Themed table — crates/ui/src/table */

#include "ui/sizing.h"
#include "ui/menu.h"
#include "base/data_table.h"

namespace gpui {

namespace component {

// One column of a DataTable: what its head says, how wide it is, and whether
// it answers to a sort or a selection.
struct TableColumn {
    Str title = {};
    float width = 100;
    bool right = false;
    bool sortable = false;
    bool selectable = true;
    // Column::resizable: whether this column's right edge takes a drag.
    bool resizable = true;
    // Column::fixed(ColumnFixed::Left): the column stays put while the rest
    // scroll under the head. TableState::colFixed is the master switch.
    bool fixed = false;
};

// ColumnGroup: one band of an upper head row, spanning that many columns of
// the row under it. Every level has to span every column, so the bands line
// up with the columns they name.
struct TableGroupCell {
    Str label = {};
    int span = 1;
};

// The themed data table. The rows are the caller's — a delegate renders a
// cell in Rust and a callback does the same here — and the table owns the
// head, the sort icons, the selection and the striping.
struct DataTable {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<TableState> state = {};
    const TableColumn* columns = nullptr;
    int nColumns = 0;
    // render_td: the cell at (row, col), built by the caller.
    El* (*cell)(Ctx* cx, void* data, int row, int col) = nullptr;
    void* data = nullptr;
    int nRows = 0;
    bool stripe = false;
    // group_headers: the extra head rows a caller stacks over the columns,
    // outermost first.
    const TableGroupCell* groupRows[4] = {};
    int groupRowLens[4] = {};
    int nGroupHeaders = 0;
    // The height the body scrolls inside. 0 leaves every row built, which is
    // what a table small enough not to need a viewport wants.
    float h = 0;
    // render_empty: what a table with no rows shows. Null takes Rust's own.
    El* empty = nullptr;
    // TableDelegate::context_menu(row_ix, menu). A secondary press marks a
    // row — which is what `right_clicked_row` is — and the table hands that
    // row and a menu of its own to the caller, which fills the menu in and
    // hands it back. The trait's default answers it back untouched, so a
    // table that sets nothing here has no menu; null is the same thing.
    PopupMenu* (*contextMenu)(Ctx* cx, void* data, int row,
                              PopupMenu* menu) = nullptr;
    // Size::table_row_height, which is what with_size(..) comes to here: 26 /
    // 30 / 32 / 40, or an explicit pixel height.
    float rowHeight = 32;

    static DataTable* New(Ctx* cx, Str id, Entity<TableState> state);
    DataTable* Columns(const TableColumn* cols, int n);
    DataTable* Rows(int n, void* data,
                    El* (*cell)(Ctx* cx, void* data, int row, int col));
    DataTable* Stripe(bool v);
    DataTable* WithSize(UiSize s);
    // Size::Size(px), which the story's 48px row offers.
    DataTable* RowHeight(float px);
    DataTable* GroupHeader(const TableGroupCell* cells, int n);
    DataTable* H(float px);
    DataTable* Empty(El* e);
    DataTable* ContextMenu(PopupMenu* (*fn)(Ctx*, void*, int, PopupMenu*));
    El* IntoEl();
    // The table itself, before the context menu is hung off it.
    El* BuildEl();
    // The width a column is drawn at: the table's own once a drag has moved
    // it, and what the caller declared until then.
    float ColWidth(const TableState* s, int col) const;
};

// The plain table of strings, which is what the simple story page shows.
struct Table {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    const char** heads = nullptr;
    int nHeads = 0;
    const char*** rows = nullptr;
    int nRows = 0;

    static Table* New(Ctx* cx);
    Table* Heads(const char** h, int n);
    Table* Rows(const char*** r, int n);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
