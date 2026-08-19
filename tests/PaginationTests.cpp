/* Ported from crates/base/src/pagination.rs, mod tests.
 *
 * Two of the four Rust cases are pure logic and come over as they are. The
 * other two need TestAppContext: one drives `request_page` through a window to
 * watch the handler fire, and the other reads an accessibility node. The
 * guards the first one is really about are `PaginationCanRequest`, which is
 * checked here without the window. */

#include "Test.h"

static void ClampsControlledValuesAndNavigationBoundaries() {
    PaginationState first = PaginationStateNew(0, 0);
    utassert(first.currentPage == 1);
    utassert(first.totalPages == 1);
    utassert(PaginationPrevPage(&first) == 0);
    utassert(PaginationNextPage(&first) == 0);

    PaginationState last = PaginationStateNew(20, 10);
    utassert(last.currentPage == 10);
    utassert(PaginationPrevPage(&last) == 9);
    utassert(PaginationNextPage(&last) == 0);

    PaginationState off = last;
    off.disabled = true;
    utassert(PaginationPrevPage(&off) == 0);
}

static void CreatesPagesAndNavigableEllipsisRanges() {
    PaginationState s = PaginationStateNew(5, 10);
    s.visiblePages = 7;
    PaginationItem items[16];
    int n = PaginationItems(&s, items, 16);
    utassert(n == 9);
    utassert(items[0].page == 1);
    // Rust's Ellipsis(2..3) is the half-open range over page 2 alone.
    utassert(items[1].page == 0);
    utassert(items[1].from == 2 && items[1].to == 2);
    utassert(items[2].page == 3);
    utassert(items[3].page == 4);
    utassert(items[4].page == 5);
    utassert(items[5].page == 6);
    utassert(items[6].page == 7);
    // Ellipsis(8..10) covers pages 8 and 9.
    utassert(items[7].page == 0);
    utassert(items[7].from == 8 && items[7].to == 9);
    utassert(items[8].page == 10);
}

static void ASinglePageHasNothingToNavigate() {
    PaginationState s = PaginationStateNew(1, 1);
    PaginationItem items[16];
    utassert(PaginationItems(&s, items, 16) == 0);
}

static void EveryPageChangeRequestIsValidated() {
    PaginationState s = PaginationStateNew(3, 5);
    utassert(!PaginationCanRequest(&s, 3));
    utassert(!PaginationCanRequest(&s, 0));
    utassert(!PaginationCanRequest(&s, 6));
    utassert(PaginationCanRequest(&s, 4));

    PaginationState off = s;
    off.disabled = true;
    utassert(!PaginationCanRequest(&off, 2));
}

void TestPagination() {
    TestSuite("pagination");
    ClampsControlledValuesAndNavigationBoundaries();
    CreatesPagesAndNavigableEllipsisRanges();
    ASinglePageHasNothingToNavigate();
    EveryPageChangeRequestIsValidated();
}
