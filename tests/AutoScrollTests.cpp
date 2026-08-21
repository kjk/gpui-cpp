/* crates/base/src/auto_scroll.rs tests */

#include "Test.h"

// delta_uses_dead_zone_and_symmetric_edge_ramps
static void TheMiddleScrollsNothingAndTheEdgesMatch() {
    // Bounds::new(point(0, 100), size(200, 100)): y 100..200.
    Bounds b = {0, 100, 200, 100};
    float d = 0;

    // The dead zone: anywhere between the two triggers moves nothing.
    utassert(!AutoScrollComputeDelta(150.f, b, &d));
    // The triggers sit sixteen inside, so the edges themselves are outside it.
    utassert(AutoScrollComputeDelta(190.f, b, &d));
    utassert(AutoScrollComputeDelta(110.f, b, &d));
    utassert(!AutoScrollComputeDelta(183.f, b, &d));
    utassert(!AutoScrollComputeDelta(117.f, b, &d));

    float top = 0, bottom = 0;
    utassert(AutoScrollComputeDelta(80.f, b, &top));
    utassert(AutoScrollComputeDelta(220.f, b, &bottom));
    // The ramp is symmetric about the box, and up is negative.
    utassertnear(top, -bottom);
    utassert(top < -kAutoScrollMinSpeed);

    // Just past a trigger is the minimum speed, and far past it is the
    // maximum — the two ends of one ramp with no flat part in it.
    utassert(AutoScrollComputeDelta(184.1f, b, &d));
    utassert(d > kAutoScrollMinSpeed && d < kAutoScrollMinSpeed + 1.f);
    utassert(AutoScrollComputeDelta(1000.f, b, &d));
    utassertnear(d, kAutoScrollMaxSpeed);
    utassert(AutoScrollComputeDelta(-1000.f, b, &d));
    utassertnear(d, -kAutoScrollMaxSpeed);
}

// stop_clears_delta_and_drag_position
static void StoppingClearsTheDeltaAndWhereTheDragWas() {
    AutoScroll s;
    utassert(!s.IsActive());
    s.Set(20.f);
    s.lastDrag = Point{1.f, 2.f};
    s.hasLastDrag = true;
    utassert(s.IsActive());

    s.Stop();
    utassert(!s.IsActive());
    utassert(!s.hasLastDrag);

    // set(None) is the softer one: the ticking stops, the drag position
    // stays, which is what a move back inside the box does mid-drag.
    s.Set(20.f);
    s.hasLastDrag = true;
    s.SetNone();
    utassert(!s.IsActive());
    utassert(s.hasLastDrag);
}

void TestAutoScroll() {
    TestSuite("auto_scroll");
    TheMiddleScrollsNothingAndTheEdgesMatch();
    StoppingClearsTheDeltaAndWhereTheDragWas();
}
