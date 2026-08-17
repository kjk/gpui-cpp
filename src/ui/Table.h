/* Unstyled table — crates/base/src/table.rs */

#pragma once

#include "gpui/Gpui.h"

namespace gpui {

struct Table {
    static El* New(Arena* a, Str id);
};
struct TableHeader {
    static El* New(Arena* a, Str id);
};
struct TableBody {
    static El* New(Arena* a, Str id);
};
struct TableRow {
    static El* New(Arena* a, Str id);
};
struct TableHead {
    static El* New(Arena* a, Str id);
};
struct TableCell {
    static El* New(Arena* a, Str id);
};
} // namespace gpui
