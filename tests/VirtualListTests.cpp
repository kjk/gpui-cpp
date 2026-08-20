/* Ported from crates/base/src/virtual_list.rs.
 *
 * The visible range is what makes a virtual list virtual, and its edges are
 * the part worth pinning: the first item is the one whose end has passed the
 * top, and the last carries a spare so the row being scrolled into is already
 * built when it arrives. Offsets here run positive-down, the way El::ScrollY
 * takes them; Rust's run negative because it offsets the content. */

#include "Test.h"

// Ten rows of fifty, in a viewport of two hundred.
static const float kRows[10] = {50, 50, 50, 50, 50, 50, 50, 50, 50, 50};

static void TheTopOfTheListStartsAtZero() {
    VirtualRange r = VirtualListVisibleRange(kRows, 10, 0, 200);
    utassert(r.first == 0);
    // Rows 0..3 fill the viewport exactly. The search for the bottom edge
    // stops at row 4 — the first whose end is past it — takes that one in,
    // and Rust then adds one more, so six are built for four on screen.
    utassert(r.end == 6);
}

static void ScrollingMovesBothEdges() {
    // Scrolled by two whole rows: rows 2..5 are on screen, and the same two
    // spares follow.
    VirtualRange r = VirtualListVisibleRange(kRows, 10, 100, 200);
    utassert(r.first == 2);
    utassert(r.end == 8);

    // Scrolled by half a row: the one straddling the top edge is still in,
    // and the one straddling the bottom brings a single spare with it.
    r = VirtualListVisibleRange(kRows, 10, 25, 200);
    utassert(r.first == 0);
    utassert(r.end == 6);
}

static void TheEndOfTheListStopsAtTheCount() {
    // Scrolled to the bottom: nothing crosses the bottom edge, so the rest of
    // the list is taken and the spare cannot run past it.
    VirtualRange r = VirtualListVisibleRange(kRows, 10, 300, 200);
    utassert(r.first == 6);
    utassert(r.end == 10);
}

static void AListShorterThanItsViewportIsAllVisible() {
    VirtualRange r = VirtualListVisibleRange(kRows, 3, 0, 500);
    utassert(r.first == 0);
    utassert(r.end == 3);
}

static void AnEmptyListHasNothingToBuild() {
    VirtualRange r = VirtualListVisibleRange(kRows, 0, 0, 200);
    utassert(r.first == 0);
    utassert(r.end == 0);
}

static void RowsOfDifferentHeightsStillLineUp() {
    const float rows[5] = {10, 100, 30, 60, 20};
    utassertnear(VirtualListItemOrigin(rows, 5, 0), 0.f);
    utassertnear(VirtualListItemOrigin(rows, 5, 1), 10.f);
    utassertnear(VirtualListItemOrigin(rows, 5, 3), 140.f);
    utassertnear(VirtualListContentSize(rows, 5), 220.f);

    // Scrolled past the tall second row, the first visible is the third.
    VirtualRange r = VirtualListVisibleRange(rows, 5, 115, 50);
    utassert(r.first == 2);
    utassert(r.end == 5);
}

static void UniformRowsAnswerTheSameRange() {
    // Ten rows of fifty in a viewport of two hundred, worked out by division
    // rather than by scanning: the same edges as the general scan.
    VirtualRange r = VirtualListVisibleRows(10, 50, 0, 200);
    utassert(r.first == 0 && r.end == 6);
    r = VirtualListVisibleRows(10, 50, 100, 200);
    utassert(r.first == 2 && r.end == 8);
    r = VirtualListVisibleRows(10, 50, 25, 200);
    utassert(r.first == 0 && r.end == 6);
    // Past the bottom the spare cannot run off the end.
    r = VirtualListVisibleRows(10, 50, 300, 200);
    utassert(r.first == 6 && r.end == 10);
    // A list shorter than its viewport is all of it, and an empty one is
    // nothing.
    r = VirtualListVisibleRows(3, 50, 0, 200);
    utassert(r.first == 0 && r.end == 3);
    r = VirtualListVisibleRows(0, 50, 0, 200);
    utassert(r.first == 0 && r.end == 0);
}

static void ScrollToItemMovesAsLittleAsItCan() {
    // Ten rows of fifty, a viewport of two hundred: rows 0..3 are on screen.
    // A row already in view does not move the list at all.
    utassertnear(VirtualListScrollToRow(10, 50, 2, 0, 200, ScrollStrategy::Top),
                 0.f);
    // One below the fold aligns with the bottom edge.
    utassertnear(VirtualListScrollToRow(10, 50, 5, 0, 200, ScrollStrategy::Top),
                 100.f);
    // One above it aligns with the top.
    utassertnear(
        VirtualListScrollToRow(10, 50, 1, 200, 200, ScrollStrategy::Bottom),
        50.f);
    // Center puts the row's middle on the viewport's middle...
    utassertnear(
        VirtualListScrollToRow(10, 50, 5, 0, 200, ScrollStrategy::Center),
        175.f);
    // ...but never past either end of the list.
    utassertnear(
        VirtualListScrollToRow(10, 50, 0, 100, 200, ScrollStrategy::Center),
        0.f);
    utassertnear(
        VirtualListScrollToRow(10, 50, 9, 0, 200, ScrollStrategy::Center),
        300.f);
    // A row that is not there leaves the offset where it was.
    utassertnear(
        VirtualListScrollToRow(10, 50, 20, 75, 200, ScrollStrategy::Top), 75.f);
}


// VirtualListScrollHandle: a request to scroll to an item waits on the handle
// until the list is laid out, which is the moment anything knows where the
// item is.
static void AScrollRequestWaitsForTheLayout() {
    VirtualListScrollHandle h;
    VirtualListScrollToItemDeferred(&h, 5, ScrollStrategy::Top);
    utassert(h.pending);
    // Nothing has moved yet: the handle does not know the sizes.
    utassertnear(h.offset, 0.f);

    utassert(VirtualListHandleLayout(&h, nullptr, 10, 50, 200));
    utassert(!h.pending);
    utassert(h.itemsCount == 10);
    utassertnear(h.viewport, 200.f);
    utassertnear(h.contentSize, 500.f);
    // Row 5 is below the view, so it comes to the bottom of it.
    utassertnear(h.offset, 100.f);

    // A layout with nothing pending leaves the offset alone.
    utassert(!VirtualListHandleLayout(&h, nullptr, 10, 50, 200));
    utassertnear(h.offset, 100.f);
}

// scroll_to_item_with_offset scrolls to the item that many past the one it
// names.
static void AnOffsetScrollsToALaterItem() {
    VirtualListScrollHandle h;
    VirtualListScrollToItemDeferredWithOffset(&h, 3, ScrollStrategy::Top, 2);
    VirtualListHandleLayout(&h, nullptr, 10, 50, 200);
    // Item 5, at the bottom of the view.
    utassertnear(h.offset, 100.f);
}

// scroll_to_bottom is the last item at the top of the view, which is as far
// as the list goes.
static void ScrollToBottomGoesAsFarAsThereIs() {
    VirtualListScrollHandle h;
    VirtualListHandleLayout(&h, nullptr, 10, 50, 200);
    VirtualListScrollToBottomDeferred(&h);
    VirtualListHandleLayout(&h, nullptr, 10, 50, 200);
    utassertnear(h.offset, 300.f);

    // An empty list has nowhere to go, and the request is still taken.
    VirtualListScrollHandle empty;
    VirtualListScrollToBottomDeferred(&empty);
    VirtualListHandleLayout(&empty, nullptr, 0, 50, 200);
    utassert(!empty.pending);
    utassertnear(empty.offset, 0.f);
}

// The clamp at the end of the layout: a list that shrank under a scrolled
// view comes back rather than showing nothing.
static void TheOffsetIsClampedToTheList() {
    VirtualListScrollHandle h;
    h.offset = 300;
    VirtualListHandleLayout(&h, nullptr, 10, 50, 200);
    utassertnear(h.offset, 300.f);
    // Four rows left, so 200px of content and nothing to scroll.
    VirtualListHandleLayout(&h, nullptr, 4, 50, 200);
    utassertnear(h.offset, 0.f);
}

// The handle answers the same range the list builds from.
static void TheHandleAnswersTheVisibleRange() {
    VirtualListScrollHandle h;
    VirtualListScrollToItemDeferred(&h, 5, ScrollStrategy::Top);
    VirtualListHandleLayout(&h, nullptr, 10, 50, 200);
    VirtualRange r = VirtualListHandleRange(&h, nullptr, 10, 50);
    VirtualRange want = VirtualListVisibleRows(10, 50, h.offset, 200);
    utassert(r.first == want.first && r.end == want.end);

    // And the same for a list whose rows are not all one height.
    const float sizes[] = {30, 40, 50, 60, 70};
    VirtualListScrollHandle var;
    VirtualListScrollToItemDeferred(&var, 4, ScrollStrategy::Top);
    VirtualListHandleLayout(&var, sizes, 5, 0, 100);
    utassertnear(var.contentSize, 250.f);
    utassertnear(var.offset, 150.f);
    r = VirtualListHandleRange(&var, sizes, 5, 0);
    want = VirtualListVisibleRange(sizes, 5, var.offset, 100);
    utassert(r.first == want.first && r.end == want.end);
}

void TestVirtualList() {
    TestSuite("virtual_list");
    TheTopOfTheListStartsAtZero();
    ScrollingMovesBothEdges();
    TheEndOfTheListStopsAtTheCount();
    AListShorterThanItsViewportIsAllVisible();
    AnEmptyListHasNothingToBuild();
    RowsOfDifferentHeightsStillLineUp();
    UniformRowsAnswerTheSameRange();
    ScrollToItemMovesAsLittleAsItCan();

    TestSuite("virtual_list/handle");
    AScrollRequestWaitsForTheLayout();
    AnOffsetScrollsToALaterItem();
    ScrollToBottomGoesAsFarAsThereIs();
    TheOffsetIsClampedToTheList();
    TheHandleAnswersTheVisibleRange();
}
