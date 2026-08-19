/* Unstyled pagination — crates/base/src/pagination.rs */

#include "gpui/gpui.h"

namespace gpui {

struct Pagination {
    static El* New(Ctx* cx, Str id);
};
struct PaginationItem {
    static El* New(Ctx* cx, Str id, int clickId = 0);
};
} // namespace gpui
