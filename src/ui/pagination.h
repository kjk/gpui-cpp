#ifndef GPUI_UI_PAGINATION_H_
#define GPUI_UI_PAGINATION_H_
/* Themed pagination — crates/ui/src/pagination.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

// What an ellipsis opens onto is the pages it stands for, and a menu reports
// the row that was taken rather than the page. The row index is only a page
// once the range's first page is beside it, which is what this keyed state
// carries between the frame that built the menu and the click that runs.
struct PaginationMenuState {
    int firstPage = 1;
    Listener onChange = {};

    static void OnItem(PaginationMenuState* self, Ctx* cx, const ClickEvent* ev,
                       intptr_t ix);
};

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
#endif // GPUI_UI_PAGINATION_H_
