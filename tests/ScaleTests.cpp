/* Ported from crates/ui/src/plot/scale/{linear,point,ordinal}.rs, mod tests.
 *
 * Rust's scales are generic and own their domain and range as Vecs; ours
 * borrow float arrays and ScaleOrdinal maps indexes. Every assertion below is
 * the reference's, with `Option<f32>` read as the bool return plus an out
 * parameter. */

#include "Test.h"

using namespace gpui::component;

// ─── ScaleLinear ──────────────────────────────────────────────────────────

static void ScaleLinearBasics() {
    const float domain[] = {1, 2, 3};
    float t = 0;

    const float up[] = {0, 100};
    ScaleLinear s = ScaleLinear::New(domain, 3, up, 2);
    utassert(s.Tick(1, &t) && TestNear(t, 0.f));
    utassert(s.Tick(2, &t) && TestNear(t, 50.f));
    utassert(s.Tick(3, &t) && TestNear(t, 100.f));

    // A descending range maps the domain backwards, which is a y axis.
    const float down[] = {100, 0};
    s = ScaleLinear::New(domain, 3, down, 2);
    utassert(s.Tick(1, &t) && TestNear(t, 100.f));
    utassert(s.Tick(2, &t) && TestNear(t, 50.f));
    utassert(s.Tick(3, &t) && TestNear(t, 0.f));
}

static void ScaleLinearMultipleRange() {
    const float domain[] = {1, 2, 3};
    float t = 0;

    // Only the ends of the range count, and only their order.
    const float up[] = {0, 50, 100};
    ScaleLinear s = ScaleLinear::New(domain, 3, up, 3);
    utassert(s.Tick(1, &t) && TestNear(t, 0.f));
    utassert(s.Tick(2, &t) && TestNear(t, 50.f));
    utassert(s.Tick(3, &t) && TestNear(t, 100.f));

    const float down[] = {100, 50, 0};
    s = ScaleLinear::New(domain, 3, down, 3);
    utassert(s.Tick(1, &t) && TestNear(t, 100.f));
    utassert(s.Tick(2, &t) && TestNear(t, 50.f));
    utassert(s.Tick(3, &t) && TestNear(t, 0.f));

    const float downUp[] = {100, 0, 100};
    s = ScaleLinear::New(domain, 3, downUp, 3);
    utassert(s.Tick(1, &t) && TestNear(t, 100.f));
    utassert(s.Tick(2, &t) && TestNear(t, 50.f));
    utassert(s.Tick(3, &t) && TestNear(t, 0.f));

    const float upDown[] = {0, 100, 0};
    s = ScaleLinear::New(domain, 3, upDown, 3);
    utassert(s.Tick(1, &t) && TestNear(t, 0.f));
    utassert(s.Tick(2, &t) && TestNear(t, 50.f));
    utassert(s.Tick(3, &t) && TestNear(t, 100.f));
}

static void ScaleLinearEmpty() {
    float t = 0;

    // No domain is no extent to divide by, so there is no tick at all.
    const float range[] = {0, 100};
    ScaleLinear s = ScaleLinear::New(nullptr, 0, range, 2);
    utassert(!s.Tick(1, &t));
    utassert(!s.Tick(2, &t));
    utassert(!s.Tick(3, &t));

    // No range still ticks; everything lands on zero.
    const float domain[] = {1, 2, 3};
    s = ScaleLinear::New(domain, 3, nullptr, 0);
    utassert(s.Tick(1, &t) && TestNear(t, 0.f));
    utassert(s.Tick(2, &t) && TestNear(t, 0.f));
    utassert(s.Tick(3, &t) && TestNear(t, 0.f));
}

static void ScaleLinearLeastIndexWithDomain() {
    const float domain[] = {1, 2, 3};
    const float range[] = {0, 100};
    ScaleLinear s = ScaleLinear::New(domain, 3, range, 2);

    int index = 0;
    float tick = 0;
    s.LeastIndexWithDomain(0, domain, 3, &index, &tick);
    utassert(index == 0);
    utassertnear(tick, 0.f);

    s.LeastIndexWithDomain(50, domain, 3, &index, &tick);
    utassert(index == 1);
    utassertnear(tick, 50.f);

    s.LeastIndexWithDomain(100, domain, 3, &index, &tick);
    utassert(index == 2);
    utassertnear(tick, 100.f);
}

// ─── ScalePoint ───────────────────────────────────────────────────────────

static void ScalePointBasics() {
    const float domain[] = {1, 2, 3};
    const float range[] = {0, 100};
    ScalePoint s = ScalePoint::New(domain, 3, range, 2);
    float t = 0;

    utassert(s.Tick(1, &t) && TestNear(t, 0.f));
    utassert(s.Tick(2, &t) && TestNear(t, 50.f));
    utassert(s.Tick(3, &t) && TestNear(t, 100.f));
}

static void ScalePointRange() {
    const float domain[] = {1, 2, 3};
    const float range[] = {40, 80};
    ScalePoint s = ScalePoint::New(domain, 3, range, 2);
    float t = 0;

    utassert(s.Tick(1, &t) && TestNear(t, 40.f));
    utassert(s.Tick(2, &t) && TestNear(t, 60.f));
    utassert(s.Tick(3, &t) && TestNear(t, 80.f));
}

static void ScalePointEmpty() {
    float t = 0;

    const float range[] = {0, 100};
    ScalePoint s = ScalePoint::New(nullptr, 0, range, 2);
    utassert(!s.Tick(1, &t));
    utassert(!s.Tick(2, &t));
    utassert(!s.Tick(3, &t));

    const float domain[] = {1, 2, 3};
    s = ScalePoint::New(domain, 3, nullptr, 0);
    utassert(s.Tick(1, &t) && TestNear(t, 0.f));
    utassert(s.Tick(2, &t) && TestNear(t, 0.f));
    utassert(s.Tick(3, &t) && TestNear(t, 0.f));
}

static void ScalePointSingle() {
    const float domain[] = {1};
    const float range[] = {0, 100};
    ScalePoint s = ScalePoint::New(domain, 1, range, 2);
    float t = 0;

    // One point has no spacing to step by, so it sits in the middle.
    utassert(s.Tick(1, &t) && TestNear(t, 50.f));
}

static void ScalePointLeastIndexBasic() {
    const float domain[] = {1, 2, 3};
    const float range[] = {0, 100};
    ScalePoint s = ScalePoint::New(domain, 3, range, 2);

    utassert(s.LeastIndex(0) == 0);
    utassert(s.LeastIndex(50) == 1);
    utassert(s.LeastIndex(100) == 2);

    utassert(s.LeastIndex(24) == 0);  // closer to 0
    utassert(s.LeastIndex(25) == 1);  // equidistant, rounds up
    utassert(s.LeastIndex(26) == 1);  // closer to 50
    utassert(s.LeastIndex(74) == 1);  // closer to 50
    utassert(s.LeastIndex(75) == 2);  // equidistant, rounds up
    utassert(s.LeastIndex(76) == 2);  // closer to 100

    utassert(s.LeastIndex(-10) == 0);  // below the range
    utassert(s.LeastIndex(150) == 2);  // above it
}

static void ScalePointLeastIndexWithOffset() {
    const float domain[] = {1, 2, 3};
    const float range[] = {40, 80};
    ScalePoint s = ScalePoint::New(domain, 3, range, 2);

    // The points are at 40, 60, 80.
    utassert(s.LeastIndex(40) == 0);
    utassert(s.LeastIndex(60) == 1);
    utassert(s.LeastIndex(80) == 2);

    utassert(s.LeastIndex(49) == 0);
    utassert(s.LeastIndex(50) == 1);
    utassert(s.LeastIndex(51) == 1);
    utassert(s.LeastIndex(69) == 1);
    utassert(s.LeastIndex(70) == 2);
    utassert(s.LeastIndex(71) == 2);

    utassert(s.LeastIndex(30) == 0);
    utassert(s.LeastIndex(100) == 2);
}

static void ScalePointLeastIndexDegenerate() {
    const float range[] = {0, 100};
    ScalePoint empty = ScalePoint::New(nullptr, 0, range, 2);
    utassert(empty.LeastIndex(0) == 0);
    utassert(empty.LeastIndex(50) == 0);
    utassert(empty.LeastIndex(100) == 0);

    const float one[] = {1};
    ScalePoint single = ScalePoint::New(one, 1, range, 2);
    utassert(single.LeastIndex(0) == 0);
    utassert(single.LeastIndex(50) == 0);
    utassert(single.LeastIndex(100) == 0);

    const float domain[] = {1, 2, 3};
    ScalePoint noRange = ScalePoint::New(domain, 3, nullptr, 0);
    utassert(noRange.LeastIndex(0) == 0);
    utassert(noRange.LeastIndex(50) == 0);
    utassert(noRange.LeastIndex(100) == 0);
}

// ─── ScaleOrdinal ─────────────────────────────────────────────────────────

static void ScaleOrdinalCycles() {
    // Rust: domain ["a".."e"], range [10, 20, 30]. Ours takes the domain index
    // the caller already has, so the assertions are on indexes 0..4 and the
    // range positions they land on.
    ScaleOrdinal s;
    s.rangeLen = 3;

    utassert(s.Map(0) == 0);
    utassert(s.Map(1) == 1);
    utassert(s.Map(2) == 2);
    utassert(s.Map(3) == 0);  // cycles back to the first
    utassert(s.Map(4) == 1);
    utassert(s.Map(-1) == -1);  // not in the domain, and no unknown set
}

static void ScaleOrdinalUnknown() {
    ScaleOrdinal s;
    s.rangeLen = 3;
    s.unknown = 0;

    utassert(s.Map(0) == 0);
    utassert(s.Map(-1) == 0);
}

static void ScaleOrdinalEmptyRange() {
    ScaleOrdinal s;
    s.rangeLen = 0;

    utassert(s.Map(0) == -1);
    utassert(s.Map(3) == -1);
}

void TestScale() {
    TestSuite("scale/linear");
    ScaleLinearBasics();
    ScaleLinearMultipleRange();
    ScaleLinearEmpty();
    ScaleLinearLeastIndexWithDomain();

    TestSuite("scale/point");
    ScalePointBasics();
    ScalePointRange();
    ScalePointEmpty();
    ScalePointSingle();
    ScalePointLeastIndexBasic();
    ScalePointLeastIndexWithOffset();
    ScalePointLeastIndexDegenerate();

    TestSuite("scale/ordinal");
    ScaleOrdinalCycles();
    ScaleOrdinalUnknown();
    ScaleOrdinalEmptyRange();
}
