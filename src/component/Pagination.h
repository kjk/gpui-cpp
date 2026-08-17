/* Themed pagination — crates/ui/src/pagination.rs */

#pragma once

#include "component/Common.h"

namespace gpui {

namespace component {

struct Pagination {
    Arena* a = nullptr;
    int page = 1;
    int total = 1;
    Func1<int> onChange;

    static Pagination* New(Arena* a, int page, int total);
    Pagination* OnChange(Func1<int> fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
