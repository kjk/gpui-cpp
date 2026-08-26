/* Unstyled table — crates/base/src/table.rs */

#include "gpui/gpui.h"

namespace gpui {

// Every part of a Rust table is a `div().id(id)` carrying a role and an index:
// the module owns accessibility and nothing else, and interaction — sorting a
// column, picking a row — belongs to whoever builds one.
//
// Identity is the half of that which does something here: it is what makes a
// part hit-test, hover and take a click. Rows and heads get it, because that
// is where the interaction is. Cells keep their name only — a hit rect per
// cell would put a box in the frame's hit list for every cell of a
// two-hundred-row table, and nothing would ever look them up.
struct Table {
    static El* New(Ctx* cx, Str id, int rowCount = -1,
                   int columnCount = -1);
};
struct TableHeader {
    static El* New(Ctx* cx, Str id);
};
struct TableBody {
    static El* New(Ctx* cx, Str id);
};
struct TableRow {
    static El* New(Ctx* cx, Str id, int rowIndex = 0);
};
struct TableHead {
    static El* New(Ctx* cx, Str id, int columnIndex = 0);
};
struct TableCell {
    static El* New(Ctx* cx, Str id, int columnIndex = 0);
};
struct TableCaption {
    static El* New(Ctx* cx, Str id);
};
} // namespace gpui
