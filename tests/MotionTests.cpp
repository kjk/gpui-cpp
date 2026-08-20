/* Ported from crates/base/src/animation.rs and the tests in motion.rs.
 *
 * Rust drives its transition through a test window and reads the value its
 * view sampled; the rule here is a function over the state and the clock, so
 * these drive it directly — same cases, same numbers: a zero duration arrives
 * at once, a changed target moves over time, a reversal carries on from where
 * it had got to rather than jumping, a delay holds the old value, a finished
 * transition stops asking for frames, and reduced motion adopts the target
 * without asking for one at all. */

#include "Test.h"

// The easing presets. ease_out_cubic is the default a Motion starts with.
static void TheEasingsAreTheCurvesRustNames() {
    utassertnear(EaseLinear(0.f), 0.f);
    utassertnear(EaseLinear(0.25f), 0.25f);
    utassertnear(EaseLinear(1.f), 1.f);

    // 1 - (1 - t)^3: past halfway before it is halfway through.
    utassertnear(EaseOutCubic(0.f), 0.f);
    utassertnear(EaseOutCubic(0.5f), 0.875f);
    utassertnear(EaseOutCubic(1.f), 1.f);

    // t^3: the mirror of it.
    utassertnear(EaseInCubic(0.5f), 0.125f);
    utassertnear(EaseInCubic(1.f), 1.f);

    // Slow at both ends, and exactly halfway at halfway.
    utassertnear(EaseInOutCubic(0.25f), 0.0625f);
    utassertnear(EaseInOutCubic(0.5f), 0.5f);
    utassertnear(EaseInOutCubic(0.75f), 0.9375f);

    // Every one of them clamps rather than running off the end of the curve.
    utassertnear(EaseOutCubic(-1.f), 0.f);
    utassertnear(EaseInCubic(2.f), 1.f);
    utassertnear(EaseInOutCubic(2.f), 1.f);
}

// GPUI's own easings, which the looping components are written against.
static void TheLoopingEasingsAreGpuisOwn() {
    utassertnear(EaseQuadratic(0.5f), 0.25f);
    // ease_in_out: quadratic at both ends, exactly halfway at halfway.
    utassertnear(EaseInOutQuad(0.25f), 0.125f);
    utassertnear(EaseInOutQuad(0.5f), 0.5f);
    utassertnear(EaseInOutQuad(0.75f), 0.875f);
    utassertnear(EaseOutQuint(0.5f), 1.f - 0.03125f);

    // bounce: the curve forwards over the first half and back over the
    // second, so it ends where it started and peaks in the middle.
    utassertnear(EaseBounce(EaseLinear, 0.f), 0.f);
    utassertnear(EaseBounce(EaseLinear, 0.25f), 0.5f);
    utassertnear(EaseBounce(EaseLinear, 0.5f), 1.f);
    utassertnear(EaseBounce(EaseLinear, 0.75f), 0.5f);
    utassertnear(EaseBounce(EaseLinear, 1.f), 0.f);
    // The skeleton's, which is that pair over ease_in_out.
    utassertnear(EaseBounceInOut(0.5f), EaseInOutQuad(1.f));
    utassertnear(EaseBounceInOut(0.25f), EaseInOutQuad(0.5f));
    // 1 - delta * 0.5 is what the skeleton makes of it: never below half.
    utassertnear(1.f - EaseBounceInOut(0.5f) * 0.5f, 0.5f);
    utassertnear(1.f - EaseBounceInOut(0.f) * 0.5f, 1.f);
}

// cubic_bezier: the y of the curve, with both ends pinned.
static void TheBezierRunsFromNothingToEverything() {
    utassertnear(CubicBezier(0.32f, 0.72f, 0.f, 1.f, 0.f), 0.f);
    utassertnear(CubicBezier(0.32f, 0.72f, 0.f, 1.f, 1.f), 1.f);
    // The dialog's curve, which is most of the way there at halfway.
    float mid = CubicBezier(0.32f, 0.72f, 0.f, 1.f, 0.5f);
    utassert(mid > 0.5f && mid < 1.f);
    // A curve whose control points sit on the diagonal is the diagonal.
    utassertnear(CubicBezier(1.f / 3, 1.f / 3, 2.f / 3, 2.f / 3, 0.5f), 0.5f);
}

static void TheLerpsAreTheThreeRustImplements() {
    utassertnear(Lerp(0.f, 10.f, 0.5f), 5.f);
    utassertnear(Lerp(10.f, 0.f, 0.25f), 7.5f);
    Point p = Lerp(Point{0, 100}, Point{50, 0}, 0.5f);
    utassertnear(p.x, 25.f);
    utassertnear(p.y, 50.f);
    Rgba c = Lerp(Rgb(0, 0, 0), Rgb(255, 100, 50), 0.5f);
    utassert(c.r == 128 && c.g == 50 && c.b == 25 && c.a == 255);
    // The far end is reached exactly, which a truncating channel would miss.
    Rgba end = Lerp(Rgb(0, 0, 0), Rgb(255, 255, 255), 1.f);
    utassert(end.r == 255 && end.g == 255 && end.b == 255);
}

// Transition::progress and ::sample.
static void ProgressWaitsOutTheDelayAndStopsAtTheEnd() {
    Motion m = MotionNew(100);
    utassertnear(MotionProgress(m, 0), 0.f);
    utassertnear(MotionProgress(m, 50), 0.5f);
    utassertnear(MotionProgress(m, 100), 1.f);
    utassertnear(MotionProgress(m, 5000), 1.f);

    m.delayMs = 50;
    utassertnear(MotionProgress(m, 50), 0.f);
    utassertnear(MotionProgress(m, 100), 0.5f);
    utassertnear(MotionProgress(m, 150), 1.f);

    // A duration of zero is over as soon as any time has passed. At exactly
    // zero it answers 0, because the delay is checked first and `elapsed <=
    // delay` is true of two zeros — Rust's order, and it costs nothing there
    // or here: a zero-duration transition never reaches this, since the value
    // is adopted outright before progress is worked out.
    utassertnear(MotionProgress(MotionNew(0), 0), 0.f);
    utassertnear(MotionProgress(MotionNew(0), 1), 1.f);

    m = MotionNew(100);
    m.ease = EaseLinear;
    utassertnear(MotionSample(m, 0.25f), 0.25f);
    m.ease = EaseOutCubic;
    utassertnear(MotionSample(m, 0.5f), 0.875f);
}

// a_zero_duration_target_change_is_immediate.
static void AZeroDurationTargetChangeIsImmediate() {
    MotionState<float> st;
    Motion m = MotionNew(0);
    MotionStep<float> first = MotionAdvance(&st, 0.f, m, 0, false);
    utassertnear(first.value, 0.f);
    MotionStep<float> next = MotionAdvance(&st, 1.f, m, 0, false);
    utassertnear(next.value, 1.f);
    utassert(!next.running);
}

// a_changed_target_transitions_over_time.
static void AChangedTargetTransitionsOverTime() {
    MotionState<float> st;
    Motion m = MotionNew(100);
    m.ease = EaseLinear;
    // The first value is adopted rather than transitioned to.
    utassertnear(MotionAdvance(&st, 0.f, m, 0, false).value, 0.f);
    // The change itself reports the value it is leaving.
    MotionStep<float> moved = MotionAdvance(&st, 10.f, m, 0, false);
    utassertnear(moved.value, 0.f);
    utassert(moved.running);
    // Half the duration later, half the way there.
    MotionStep<float> half = MotionAdvance(&st, 10.f, m, 0.05, false);
    utassertnear(half.value, 5.f);
    utassert(half.running);
}

// reversing_uses_the_current_sample_without_jumping.
static void ReversingCarriesOnFromWhereItGotTo() {
    MotionState<float> st;
    Motion m = MotionNew(100);
    m.ease = EaseLinear;
    MotionAdvance(&st, 0.f, m, 0, false);
    MotionAdvance(&st, 10.f, m, 0, false);
    // Halfway there, and told to go back: it leaves from 5, not from 10.
    utassertnear(MotionAdvance(&st, 0.f, m, 0.05, false).value, 5.f);
    // A quarter of the duration into the way back, a quarter of the way down.
    utassertnear(MotionAdvance(&st, 0.f, m, 0.075, false).value, 3.75f);
}

// delay_holds_the_previous_value_before_interpolation.
static void ADelayHoldsThePreviousValue() {
    MotionState<float> st;
    Motion m = MotionNew(100);
    m.delayMs = 50;
    m.ease = EaseLinear;
    MotionAdvance(&st, 0.f, m, 0, false);
    utassertnear(MotionAdvance(&st, 10.f, m, 0, false).value, 0.f);
    // Still inside the delay.
    utassertnear(MotionAdvance(&st, 10.f, m, 0.05, false).value, 0.f);
    // Half the duration past it.
    utassertnear(MotionAdvance(&st, 10.f, m, 0.1, false).value, 5.f);
}

// a_completed_transition_stops_requesting_frames.
static void AFinishedTransitionStopsAskingForFrames() {
    MotionState<float> st;
    Motion m = MotionNew(100);
    m.ease = EaseLinear;
    MotionAdvance(&st, 0.f, m, 0, false);
    utassert(MotionAdvance(&st, 1.f, m, 0, false).running);
    MotionStep<float> done = MotionAdvance(&st, 1.f, m, 0.1, false);
    utassertnear(done.value, 1.f);
    utassert(!done.running);
    // And it keeps not asking.
    utassert(!MotionAdvance(&st, 1.f, m, 0.2, false).running);
    // A value that never moved does not ask either.
    MotionState<float> still;
    MotionAdvance(&still, 3.f, m, 0, false);
    utassert(!MotionAdvance(&still, 3.f, m, 1.0, false).running);
}

// reduced_motion_adopts_the_target_without_requesting_a_frame.
static void ReducedMotionTakesTheTargetOutright() {
    MotionState<float> st;
    Motion m = MotionNew(100);
    m.ease = EaseLinear;
    MotionAdvance(&st, 0.f, m, 0, true);
    MotionStep<float> step = MotionAdvance(&st, 1.f, m, 0, true);
    utassertnear(step.value, 1.f);
    utassert(!step.running);
    // Turned on partway through a transition, the next frame arrives rather
    // than finishing the curve.
    MotionState<float> mid;
    MotionAdvance(&mid, 0.f, m, 0, false);
    MotionAdvance(&mid, 10.f, m, 0, false);
    utassertnear(MotionAdvance(&mid, 10.f, m, 0.05, true).value, 10.f);
}

// The other two types go the same way round; a color is what a fill fades
// through, and a point what something slides along.
static void TheSameRuleCarriesAPointAndAColor() {
    MotionState<Point> pt;
    Motion m = MotionNew(100);
    m.ease = EaseLinear;
    MotionAdvance(&pt, Point{0, 0}, m, 0, false);
    MotionAdvance(&pt, Point{100, 40}, m, 0, false);
    MotionStep<Point> half = MotionAdvance(&pt, Point{100, 40}, m, 0.05, false);
    utassertnear(half.value.x, 50.f);
    utassertnear(half.value.y, 20.f);

    MotionState<Rgba> col;
    MotionAdvance(&col, Rgb(0, 0, 0), m, 0, false);
    MotionAdvance(&col, Rgb(100, 100, 100), m, 0, false);
    MotionStep<Rgba> mid = MotionAdvance(&col, Rgb(100, 100, 100), m, 0.05, false);
    utassert(mid.value.r == 50);
    utassert(mid.running);
}

// The id is the element and the channel together, so one element can move two
// values at once without them sharing a slot.
static void AChannelKeepsTwoValuesOfOneElementApart() {
    utassert(MotionId(StrL("terms")) == MotionId(StrL("terms")));
    utassert(MotionId(StrL("terms"), StrL("fill")) !=
             MotionId(StrL("terms"), StrL("mark-opacity")));
    utassert(MotionId(StrL("terms"), StrL("fill")) !=
             MotionId(StrL("other"), StrL("fill")));
}

// Style::opacity, which is what a fade is made of: GPUI multiplies every
// colour a primitive paints by the opacity in force, and nested opacities
// multiply, so a subtree fades as one thing rather than each box separately.
static void OpacityMultipliesEveryColourItPaints() {
    PaintCtx ctx;
    utassert(ctx.opacity == 1.f);
    // Untouched at 1, whatever the colour's own alpha is.
    Rgba c = Rgba8(10, 20, 30, 200);
    Rgba same = PaintFade(&ctx, c);
    utassert(same.a == 200 && same.r == 10);

    // Half of a colour's alpha, with the channels left alone — the same thing
    // `Hsla::opacity` does to a colour, done to the primitive instead.
    ctx.opacity = 0.5f;
    Rgba half = PaintFade(&ctx, c);
    utassert(half.a == 100);
    utassert(half.r == 10 && half.g == 20 && half.b == 30);

    // A colour that was already translucent fades from where it was.
    Rgba faint = PaintFade(&ctx, Rgba8(0, 0, 0, 20));
    utassert(faint.a == 10);

    // Nothing at zero, everything back at one.
    ctx.opacity = 0.f;
    utassert(PaintFade(&ctx, c).a == 0);
    ctx.opacity = 1.f;
    utassert(PaintFade(&ctx, c).a == 200);

    // El::Opacity clamps rather than letting a stray value through: a
    // transition that overshoots cannot make something more opaque than it is.
    Arena* a = ArenaNew();
    El* e = Div(a)->Opacity(1.5f);
    utassertnear(e->style.opacity, 1.f);
    e->Opacity(-0.2f);
    utassertnear(e->style.opacity, 0.f);
    e->Opacity(0.25f);
    utassertnear(e->style.opacity, 0.25f);
    ArenaDelete(a);
}

void TestMotion() {
    TestSuite("motion");
    TheEasingsAreTheCurvesRustNames();
    TheLoopingEasingsAreGpuisOwn();
    TheBezierRunsFromNothingToEverything();
    TheLerpsAreTheThreeRustImplements();
    ProgressWaitsOutTheDelayAndStopsAtTheEnd();
    AZeroDurationTargetChangeIsImmediate();
    AChangedTargetTransitionsOverTime();
    ReversingCarriesOnFromWhereItGotTo();
    ADelayHoldsThePreviousValue();
    AFinishedTransitionStopsAskingForFrames();
    ReducedMotionTakesTheTargetOutright();
    TheSameRuleCarriesAPointAndAColor();
    AChannelKeepsTwoValuesOfOneElementApart();
    OpacityMultipliesEveryColourItPaints();
}
