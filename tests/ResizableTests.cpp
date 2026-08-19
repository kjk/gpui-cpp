/* Ported from crates/base/src/resizable/mod.rs.
 *
 * Rust's two cases there drive a window and a drag; both are checking
 * resize_panel_at_handle, which the drag and the programmatic API share. Its
 * numbers come over directly: two panels of 200 in a 400 container, one
 * resized to 220, leaving 220 and 180. */

#include "Test.h"

static void ResizingOnePanelTakesFromTheNext() {
    float sizes[2] = {200, 200};
    utassert(ResizablePanelResize(sizes, nullptr, nullptr, 2, 0, 220, 400));
    utassertnear(sizes[0], 220.f);
    utassertnear(sizes[1], 180.f);
}

static void TheLastPanelHasNoHandle() {
    float sizes[2] = {200, 200};
    // The handle sits between ix and ix + 1, so there is none below the last.
    utassert(!ResizablePanelResize(sizes, nullptr, nullptr, 2, 1, 300, 400));
    utassertnear(sizes[0], 200.f);
    utassertnear(sizes[1], 200.f);
    // And no move is no resize.
    utassert(!ResizablePanelResize(sizes, nullptr, nullptr, 2, 0, 200, 400));
}

static void GrowingWalksOnPastANeighbourThatIsSpent() {
    // Three panels of 200; the middle one can only give 100 before it hits
    // the default minimum, so the last one gives the rest.
    float sizes[3] = {200, 200, 200};
    utassert(ResizablePanelResize(sizes, nullptr, nullptr, 3, 0, 350, 600));
    utassertnear(sizes[0], 350.f);
    utassertnear(sizes[1], 100.f);
    utassertnear(sizes[2], 150.f);
}

static void APanelWillNotShrinkBelowItsMinimum() {
    float sizes[2] = {200, 200};
    // 40 is under the default minimum of 100, so it stops there.
    utassert(ResizablePanelResize(sizes, nullptr, nullptr, 2, 0, 40, 400));
    utassertnear(sizes[0], 100.f);
    // Rust hands the neighbour what the drag actually asked for, less what
    // the panels before could not absorb; the first panel has nothing before
    // it, so the full remainder stays with it.
    utassertnear(sizes[0] + sizes[1], 400.f);
}

static void ARangeOfItsOwnBeatsTheDefault() {
    float sizes[2] = {200, 200};
    const float mins[2] = {150, 100};
    const float maxs[2] = {250, 1e9f};
    utassert(ResizablePanelResize(sizes, mins, maxs, 2, 0, 400, 400));
    // Clamped to its own ceiling rather than the space available.
    utassertnear(sizes[0], 250.f);
    utassertnear(sizes[1], 150.f);
}

static void EveryPanelKeepsItsShareWhenTheContainerChanges() {
    float sizes[3] = {100, 200, 100};
    ResizableAdjustToContainer(sizes, 3, 800);
    utassertnear(sizes[0], 200.f);
    utassertnear(sizes[1], 400.f);
    utassertnear(sizes[2], 200.f);
    // A container of nothing leaves them alone rather than dividing by zero.
    ResizableAdjustToContainer(sizes, 3, 0);
    utassertnear(sizes[1], 400.f);
}

void TestResizable() {
    TestSuite("resizable");
    ResizingOnePanelTakesFromTheNext();
    TheLastPanelHasNoHandle();
    GrowingWalksOnPastANeighbourThatIsSpent();
    APanelWillNotShrinkBelowItsMinimum();
    ARangeOfItsOwnBeatsTheDefault();
    EveryPanelKeepsItsShareWhenTheContainerChanges();
}
