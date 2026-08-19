/* Themed pagination — crates/ui/src/pagination.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

struct Pagination {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    int page = 1;
    int total = 1;
    // How many page buttons stay visible before the list collapses to
    // ellipses; crates/base clamps this to 5.
    int visiblePages = 5;
    bool compact = false;
    bool disabled = false;
    UiSize size = UiSize::Medium;
    Listener onChange;

    static Pagination* New(Ctx* cx, int page, int total);
    Pagination* Id(Str s);
    Pagination* VisiblePages(int n);
    Pagination* Compact(bool v = true);
    Pagination* Disabled(bool v);
    Pagination* WithSize(UiSize s);
    Pagination* OnChange(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
