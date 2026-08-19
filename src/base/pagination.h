/* Unstyled pagination — crates/base/src/pagination.rs */

#include "gpui/gpui.h"

namespace gpui {

// Rust's PaginationItem: a destination is either a page or the range an
// ellipsis stands for. `page` is 0 for the ellipsis, and `from`..`to` is then
// the inclusive span it covers — Rust's half-open Range<usize> written the way
// this tree counts.
struct PaginationItem {
    int page = 0;
    int from = 0;
    int to = 0;
};

// The controlled behavior every part of a pagination control shares. Rust
// keeps the guards here rather than in each button, so the same bounds,
// disabled and same-page checks apply wherever a page is requested.
struct PaginationState {
    int currentPage = 1;
    int totalPages = 1;
    int visiblePages = 5;
    bool disabled = false;
};

// Clamps the pair the way PaginationState::new does: at least one page, and a
// current page inside it.
PaginationState PaginationStateNew(int currentPage, int totalPages);

// previous_page / next_page: 0 where Rust answers None, which a disabled
// control and either end both do.
int PaginationPrevPage(const PaginationState* s);
int PaginationNextPage(const PaginationState* s);

// request_page's guards, without the call: disabled, the page it is already
// on, and anything outside 1..=totalPages are all refused. A caller attaches
// its handler only where this says yes.
bool PaginationCanRequest(const PaginationState* s, int page);

// items(): the first and last page always show, with a window around the
// current one and an ellipsis for each gap. Returns how many were written.
int PaginationItems(const PaginationState* s, PaginationItem* out, int cap);

// The navigation landmark. Identity and nothing else; every destination in it
// is the caller's own element.
struct Pagination {
    static El* New(Ctx* cx, Str id);
};
} // namespace gpui
