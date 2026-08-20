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

    utassert(s.LeastIndex(24) == 0); // closer to 0
    utassert(s.LeastIndex(25) == 1); // equidistant, rounds up
    utassert(s.LeastIndex(26) == 1); // closer to 50
    utassert(s.LeastIndex(74) == 1); // closer to 50
    utassert(s.LeastIndex(75) == 2); // equidistant, rounds up
    utassert(s.LeastIndex(76) == 2); // closer to 100

    utassert(s.LeastIndex(-10) == 0); // below the range
    utassert(s.LeastIndex(150) == 2); // above it
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
    utassert(s.Map(3) == 0); // cycles back to the first
    utassert(s.Map(4) == 1);
    utassert(s.Map(-1) == -1); // not in the domain, and no unknown set
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

// crates/ui/src/plot/scale/band.rs: test_scale_band.
static void ScaleBandThirds() {
    const float range[2] = {0.f, 90.f};
    ScaleBand b = ScaleBand::New(3, range, 2);
    float t = 0;
    utassert(b.Tick(0, &t) && TestNear(t, 0.f));
    utassert(b.Tick(1, &t) && TestNear(t, 30.f));
    utassert(b.Tick(2, &t) && TestNear(t, 60.f));
    utassertnear(b.BandWidth(), 30.f);
}

// test_scale_band_zero: an empty domain has no bands, and an empty range has
// no width to give them.
static void ScaleBandEmpty() {
    const float range[2] = {0.f, 90.f};
    ScaleBand none = ScaleBand::New(0, range, 2);
    float t = 0;
    utassert(!none.Tick(0, &t));
    utassert(!none.Tick(1, &t));
    utassertnear(none.BandWidth(), 0.f);

    ScaleBand noRange = ScaleBand::New(3, nullptr, 0);
    utassert(noRange.Tick(0, &t) && TestNear(t, 0.f));
    utassert(noRange.Tick(1, &t) && TestNear(t, 0.f));
    utassert(noRange.Tick(2, &t) && TestNear(t, 0.f));
    utassertnear(noRange.BandWidth(), 0.f);
}

static void ScaleBandPadding() {
    const float range[2] = {0.f, 100.f};
    ScaleBand b = ScaleBand::New(4, range, 2);
    b.paddingInner = 0.2f;
    // A band gives a fifth of itself to the gap beside it.
    utassertnear(b.BandWidth(), 20.f);
    float t = 0;
    utassert(b.Tick(0, &t) && TestNear(t, 0.f));
    // The rest are spread by the ratio the inner padding works out to: a
    // quarter of the range each, stretched by 1 + 0.2/3.
    utassert(b.Tick(3, &t) && TestNear(t, 80.f));
}

static void ScaleBandSingle() {
    const float range[2] = {0.f, 90.f};
    ScaleBand b = ScaleBand::New(1, range, 2);
    float t = 0;
    // One band sits in the middle: the width is capped at thirty, so it
    // starts thirty in.
    utassert(b.Tick(0, &t) && TestNear(t, 30.f));
    utassert(b.LeastIndex(80.f) == 0);
}

static void ScaleBandLeastIndex() {
    const float range[2] = {0.f, 90.f};
    ScaleBand b = ScaleBand::New(3, range, 2);
    utassert(b.LeastIndex(0.f) == 0);
    utassert(b.LeastIndex(31.f) == 1);
    utassert(b.LeastIndex(59.f) == 2);
    // And it never runs off either end.
    utassert(b.LeastIndex(-40.f) == 0);
    utassert(b.LeastIndex(400.f) == 2);
}

// The tooltip box hugs the cursor and flips toward the middle past halfway,
// which is what keeps it inside the plot.
static void PlotTooltipQuadrants() {
    Size within = {200, 100};
    Size box = {40, 20};
    // Top left quarter: down and to the right of the cursor.
    Point at = PlotTooltipPlace({10, 10}, within, box, 8);
    utassert(TestNear(at.x, 18.f) && TestNear(at.y, 18.f));
    // Right half: the box's right edge is what hugs the cursor.
    at = PlotTooltipPlace({150, 10}, within, box, 8);
    utassert(TestNear(at.x, 102.f) && TestNear(at.y, 18.f));
    // Bottom half: it sits above.
    at = PlotTooltipPlace({10, 80}, within, box, 8);
    utassert(TestNear(at.x, 18.f) && TestNear(at.y, 52.f));
    at = PlotTooltipPlace({150, 80}, within, box, 8);
    utassert(TestNear(at.x, 102.f) && TestNear(at.y, 52.f));
}

// A box too big for the plot to hold either way still starts inside it.
static void PlotTooltipClamps() {
    Size within = {200, 100};
    Point at = PlotTooltipPlace({190, 90}, within, {400, 400}, 8);
    utassert(TestNear(at.x, 0.f) && TestNear(at.y, 0.f));
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

    TestSuite("scale/band");
    ScaleBandThirds();
    ScaleBandEmpty();
    ScaleBandPadding();
    ScaleBandSingle();
    ScaleBandLeastIndex();

    TestSuite("plot/tooltip");
    PlotTooltipQuadrants();
    PlotTooltipClamps();
}
