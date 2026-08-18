/* Themed pagination — crates/ui/src/pagination.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

struct Pagination {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    int page = 1;
    int total = 1;
    Listener onChange;

    static Pagination* New(Ctx* cx, int page, int total);
    Pagination* OnChange(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
