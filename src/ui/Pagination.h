/* Unstyled pagination — crates/base/src/pagination.rs */

#include "gpui/Gpui.h"

namespace gpui {

struct Pagination {
    static El* New(Arena* a, Str id);
};
struct PaginationItem {
    static El* New(Arena* a, Str id, int clickId = 0);
};
} // namespace gpui
