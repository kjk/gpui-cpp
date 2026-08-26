/* Ported from crates/base/src/geometry.rs and the pinned base-compat test. */

#include "Test.h"

static void PlacementsReportTheirOrientationAndAxis() {
    utassert(PlacementIsVertical(Placement::Top));
    utassert(PlacementIsVertical(Placement::Bottom));
    utassert(!PlacementIsVertical(Placement::Left));
    utassert(PlacementIsHorizontal(Placement::Left));
    utassert(PlacementIsHorizontal(Placement::Right));
    utassert(!PlacementIsHorizontal(Placement::Top));
    utassert(PlacementAxis(Placement::Top) == Axis::Vertical);
    utassert(PlacementAxis(Placement::Bottom) == Axis::Vertical);
    utassert(PlacementAxis(Placement::Left) == Axis::Horizontal);
    utassert(PlacementAxis(Placement::Right) == Axis::Horizontal);
}

static void SidesAndAxesExposeBothDirections() {
    utassert(SideIsLeft(Side::Left));
    utassert(!SideIsLeft(Side::Right));
    utassert(SideIsRight(Side::Right));
    utassert(!SideIsRight(Side::Left));
    utassert(AxisIsHorizontal(Axis::Horizontal));
    utassert(!AxisIsHorizontal(Axis::Vertical));
    utassert(AxisIsVertical(Axis::Vertical));
    utassert(!AxisIsVertical(Axis::Horizontal));
}

static void AllEdgesReceiveTheSameValue() {
    Edges e = EdgesAll(7.f);
    utassertnear(e.top, 7.f);
    utassertnear(e.right, 7.f);
    utassertnear(e.bottom, 7.f);
    utassertnear(e.left, 7.f);
}

void TestGeometry() {
    TestSuite("geometry");
    PlacementsReportTheirOrientationAndAxis();
    SidesAndAxesExposeBothDirections();
    AllEdgesReceiveTheSameValue();
}
