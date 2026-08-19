/* Themed table — crates/ui/src/table */

#include "ui/sizing.h"
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
    // The extra head rows a caller stacks over the columns.
    El* groupHeaders[4] = {};
    int nGroupHeaders = 0;

    static DataTable* New(Ctx* cx, Str id, Entity<TableState> state);
    DataTable* Columns(const TableColumn* cols, int n);
    DataTable* Rows(int n, void* data,
                    El* (*cell)(Ctx* cx, void* data, int row, int col));
    DataTable* Stripe(bool v);
    DataTable* GroupHeader(El* el);
    El* IntoEl();
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
