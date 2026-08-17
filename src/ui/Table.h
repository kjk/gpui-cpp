/* Unstyled table — crates/base/src/table.rs */

#include "gpui/Gpui.h"

namespace gpui {

struct Table {
    static El* New(Ctx* cx, Str id);
};
struct TableHeader {
    static El* New(Ctx* cx, Str id);
};
struct TableBody {
    static El* New(Ctx* cx, Str id);
};
struct TableRow {
    static El* New(Ctx* cx, Str id);
};
struct TableHead {
    static El* New(Ctx* cx, Str id);
};
struct TableCell {
    static El* New(Ctx* cx, Str id);
};
} // namespace gpui
