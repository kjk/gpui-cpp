/* Unstyled pagination — crates/base/src/pagination.rs */

#pragma once

#include "gpui/Gpui.h"

struct Pagination {
    static El* New(Arena* a, Str id);
};
struct PaginationItem {
    static El* New(Arena* a, Str id, int clickId = 0);
};
