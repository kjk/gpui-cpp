#include "base/pagination.h"

namespace gpui {

PaginationState PaginationStateNew(int currentPage, int totalPages) {
    PaginationState s = {};
    s.totalPages = totalPages < 1 ? 1 : totalPages;
    s.currentPage = currentPage < 1              ? 1
                    : currentPage > s.totalPages ? s.totalPages
                                                 : currentPage;
    return s;
}

int PaginationPrevPage(const PaginationState* s) {
    if (s->disabled || s->currentPage <= 1) {
        return 0;
    }
    return s->currentPage - 1;
}

int PaginationNextPage(const PaginationState* s) {
    if (s->disabled || s->currentPage >= s->totalPages) {
        return 0;
    }
    return s->currentPage + 1;
}

bool PaginationCanRequest(const PaginationState* s, int page) {
    if (s->disabled || page == s->currentPage) {
        return false;
    }
    return page >= 1 && page <= s->totalPages;
}

int PaginationItems(const PaginationState* s, PaginationItem* out, int cap) {
    int current = s->currentPage;
    int total = s->totalPages;
    int n = 0;
    if (total <= 1) {
        return 0;
    }
    int maxVisible = s->visiblePages < 5 ? 5 : s->visiblePages;
    if (total <= maxVisible) {
        for (int i = 1; i <= total && n < cap; i++) {
            out[n].page = i;
            n++;
        }
        return n;
    }
    out[n].page = 1;
    n++;
    int side = (maxVisible - 3) / 2;
    int start = current <= side + 1          ? 2
                : current > total - side - 1 ? total - side - 1
                                             : current - side;
    if (start > 2 && n < cap) {
        out[n].page = 0;
        out[n].from = 2;
        out[n].to = start - 1;
        n++;
    }
    int end = current >= total - side ? total - 1
              : current <= side + 1   ? side + 2
                                      : current + side;
    for (int i = start; i <= end && n < cap; i++) {
        out[n].page = i;
        n++;
    }
    if (end < total - 1 && n < cap) {
        out[n].page = 0;
        out[n].from = end + 1;
        out[n].to = total - 1;
        n++;
    }
    if (n < cap) {
        out[n].page = total;
        n++;
    }
    return n;
}

El* Pagination::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)
        ->Id(id)
        ->Role(AccessibilityRole::Navigation)
        ->AriaLabel(StrL("Pagination"));
}
} // namespace gpui
