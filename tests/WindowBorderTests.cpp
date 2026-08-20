/* Ported from crates/ui/src/window_border.rs.
 *
 * A client-decorated window keeps a band of shadow padding around its frame,
 * and the band a press counts as a resize straddles that inner frame. A side
 * the window manager has tiled flush against something has neither. */

#include "Test.h"

using namespace gpui::component;

static void TilingTakesTheShadowOffThatSide() {
    WindowTiling none;
    Edges e = WindowBorderInsets(20, none);
    utassertnear(e.top, 20.f);
    utassertnear(e.bottom, 20.f);
    utassertnear(e.left, 20.f);
    utassertnear(e.right, 20.f);

    WindowTiling leftTiled;
    leftTiled.left = true;
    e = WindowBorderInsets(20, leftTiled);
    utassertnear(e.left, 0.f);
    utassertnear(e.right, 20.f);

    WindowTiling all;
    all.top = all.bottom = all.left = all.right = true;
    e = WindowBorderInsets(20, all);
    utassertnear(e.top, 0.f);
    utassertnear(e.bottom, 0.f);
    utassertnear(e.left, 0.f);
    utassertnear(e.right, 0.f);
    utassert(all.AllTiled());
    utassert(!none.IsTiled());
}

// A 200x100 window with 20 of shadow: the frame runs from 20,20 to 180,80.
static void EachEdgeIsGrabbedAlongItsOwnSegment() {
    WindowTiling none;
    Edges insets = WindowBorderInsets(20, none);
    const float hit = 4;

    utassert(WindowResizeEdge(20, 50, 200, 100, insets, none, hit) ==
             WindowEdge::Left);
    utassert(WindowResizeEdge(180, 50, 200, 100, insets, none, hit) ==
             WindowEdge::Right);
    utassert(WindowResizeEdge(100, 20, 200, 100, insets, none, hit) ==
             WindowEdge::Top);
    utassert(WindowResizeEdge(100, 80, 200, 100, insets, none, hit) ==
             WindowEdge::Bottom);
    // The corners win over the sides they are made of.
    utassert(WindowResizeEdge(20, 20, 200, 100, insets, none, hit) ==
             WindowEdge::TopLeft);
    utassert(WindowResizeEdge(180, 20, 200, 100, insets, none, hit) ==
             WindowEdge::TopRight);
    utassert(WindowResizeEdge(20, 80, 200, 100, insets, none, hit) ==
             WindowEdge::BottomLeft);
    utassert(WindowResizeEdge(180, 80, 200, 100, insets, none, hit) ==
             WindowEdge::BottomRight);
    // Inside the frame, and out in the shadow past the corner, are neither.
    utassert(WindowResizeEdge(100, 50, 200, 100, insets, none, hit) ==
             WindowEdge::None);
    utassert(WindowResizeEdge(5, 5, 200, 100, insets, none, hit) ==
             WindowEdge::None);
    // A left edge does not run on down the extension line of the padding.
    utassert(WindowResizeEdge(20, 95, 200, 100, insets, none, hit) ==
             WindowEdge::None);
}

static void ATiledSideIsNeverGrabbed() {
    WindowTiling tiling;
    tiling.left = true;
    Edges insets = WindowBorderInsets(20, tiling);
    const float hit = 4;
    // The left edge is flush against something, so its band is gone — and
    // with it the two corners it was half of.
    utassert(WindowResizeEdge(0, 50, 200, 100, insets, tiling, hit) ==
             WindowEdge::None);
    utassert(WindowResizeEdge(0, 20, 200, 100, insets, tiling, hit) ==
             WindowEdge::Top);
    utassert(WindowResizeEdge(180, 50, 200, 100, insets, tiling, hit) ==
             WindowEdge::Right);
}

static void AWindowWithNoShadowIsGrabbedAtItsOwnEdges() {
    // What an undecorated window is: the frame is the whole box, so the band
    // straddles the client area's edges.
    WindowTiling none;
    Edges insets = WindowBorderInsets(0, none);
    const float hit = 4;
    utassert(WindowResizeEdge(1, 50, 200, 100, insets, none, hit) ==
             WindowEdge::Left);
    utassert(WindowResizeEdge(199, 99, 200, 100, insets, none, hit) ==
             WindowEdge::BottomRight);
    utassert(WindowResizeEdge(100, 50, 200, 100, insets, none, hit) ==
             WindowEdge::None);
    // The directions are numbered the way _NET_WM_MOVERESIZE numbers them,
    // which is what the X11 window sends.
    static_assert((int)WindowEdge::TopLeft == 0, "clockwise from top-left");
    static_assert((int)WindowEdge::Right == 3, "clockwise from top-left");
    static_assert((int)WindowEdge::Left == 7, "clockwise from top-left");
    static_assert((int)WindowEdge::None == -1, "no edge");
}

void TestWindowBorder() {
    TestSuite("window_border");
    TilingTakesTheShadowOffThatSide();
    EachEdgeIsGrabbedAlongItsOwnSegment();
    ATiledSideIsNeverGrabbed();
    AWindowWithNoShadowIsGrabbedAtItsOwnEdges();
}
