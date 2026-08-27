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
    float mid = CubicBezier(1.f / 3, 0.72f, 2.f / 3, 1.f, 0.5f);
    utassert(mid > 0.5f && mid < 1.f);
    // A curve whose control points sit on the diagonal is the diagonal.
    utassertnear(CubicBezier(1.f / 3, 1.f / 3, 2.f / 3, 2.f / 3, 0.5f), 0.5f);
}

// It is CSS's cubic-bezier: `x(s) = t` is solved for the curve parameter
// before y is sampled. These are animation.rs's own cases — the published
// values of the CSS `ease` curve, and Chromium's own expectations for
// (0.25, 0, 0.75, 1) out of cubic_bezier_unittest.cc.
static bool BezierNear(float got, float want, float eps) {
    float d = got - want;
    return (d < 0 ? -d : d) < eps;
}

static void TheBezierMatchesWhatAnEngineSays() {
    struct Sample {
        float t;
        float y;
    };
    const Sample ease[] = {{0.f, 0.f},   {0.2f, 0.295f}, {0.5f, 0.802f},
                           {0.8f, 0.976f}, {1.f, 1.f}};
    for (const Sample& s : ease) {
        utassert(BezierNear(CubicBezier(0.25f, 0.1f, 0.25f, 1.f, s.t), s.y,
                            1e-3f));
    }

    const Sample chromium[] = {
        {0.05f, 0.01136f}, {0.1f, 0.03978f},  {0.15f, 0.07978f},
        {0.2f, 0.12803f},  {0.25f, 0.18235f}, {0.3f, 0.24115f},
        {0.35f, 0.30323f}, {0.4f, 0.36761f},  {0.45f, 0.43345f},
        {0.5f, 0.5f},      {0.6f, 0.63238f},  {0.65f, 0.69676f},
        {0.7f, 0.75884f},  {0.75f, 0.81764f}, {0.8f, 0.87196f},
        {0.85f, 0.92021f}, {0.9f, 0.96021f},  {0.95f, 0.98863f},
    };
    for (const Sample& s : chromium) {
        utassert(BezierNear(CubicBezier(0.25f, 0.f, 0.75f, 1.f, s.t), s.y,
                            3e-4f));
    }
}

// x1 = 1/3, x2 = 2/3 collapse the x solve to the identity, so the answer is
// the plain y polynomial. The dialog leans on that to keep the trajectory it
// was tuned with.
static void ThirdsMapTimeIdentically() {
    for (int step = 0; step <= 100; step++) {
        float t = (float)step / 100.f;
        float oneT = 1.f - t;
        float want = 3.f * 0.72f * oneT * oneT * t + 3.f * oneT * t * t +
                     t * t * t;
        utassert(
            BezierNear(CubicBezier(1.f / 3, 0.72f, 2.f / 3, 1.f, t), want,
                       1e-4f));
    }
}

static void TheLerpsAreTheThreeRustImplements() {
    utassertnear(Lerp(0.f, 10.f, 0.5f), 5.f);
    utassertnear(Lerp(10.f, 0.f, 0.25f), 7.5f);
    Point p = Lerp(Point{0, 100}, Point{50, 0}, 0.5f);
    utassertnear(p.x, 25.f);
    utassertnear(p.y, 50.f);
    // The colour walks in HSL, the way `impl Lerp for Hsla` does: all four
    // channels straight, so halfway from black to a saturated orange is half
    // of each of that orange's — hue included, since black's is nothing.
    // ColorTests has the rest of the rule.
    Hsla to = HslaFromRgba(Rgb(255, 100, 50));
    Rgba c = Lerp(Rgb(0, 0, 0), Rgb(255, 100, 50), 0.5f);
    Hsla mid = HslaFromRgba(c);
    utassertnear(mid.h, to.h * 0.5f);
    utassertnear(mid.l, to.l * 0.5f);
    utassert(c.a == 255);
    // The far end is the colour itself, which a round trip through HSL would
    // miss by a byte.
    Rgba end = Lerp(Rgb(0, 0, 0), Rgb(255, 255, 255), 1.f);
    utassert(end.r == 255 && end.g == 255 && end.b == 255);
}

static void EffectTransitionAppliesEveryUpstreamEffect() {
    App app;
    Window* win = new Window();
    Arena* arena = ArenaNew();
    win->app = &app;
    Ctx cx = {&app, win, arena, {}};

    win->frameNow = 10.0;
    El* first = Div(arena);
    Transition::New(&cx, 100)
        ->Ease(EaseLinear)
        ->SlideY(-4, 0)
        ->SlideX(8, 0)
        ->Fade(0, 1)
        ->Width(20, 40)
        ->Height(10, 30)
        ->Apply(first, StrL("entrance"));
    utassertnear(first->style.absTop, -4);
    utassertnear(first->style.absLeft, 8);
    utassertnear(first->style.opacity, 0);
    utassertnear(first->style.width, 20);
    utassertnear(first->style.height, 10);

    // The element tree and builder are new next frame, while the keyed
    // animation state survives on the window exactly as with_animation does.
    win->frameNow = 10.05;
    El* half = Div(arena);
    EffectTransition::New(&cx, 100)
        ->Ease(EaseLinear)
        ->SlideY(-4, 0)
        ->SlideX(8, 0)
        ->Fade(0, 1)
        ->Width(20, 40)
        ->Height(10, 30)
        ->Apply(half, StrL("entrance"));
    utassertnear(half->style.absTop, -2);
    utassertnear(half->style.absLeft, 4);
    utassertnear(half->style.opacity, 0.5f);
    utassertnear(half->style.width, 30);
    utassertnear(half->style.height, 20);

    // The SmallVec upstream can grow; the arena-backed port does too.
    EffectTransition* many = EffectTransition::New(&cx, 100);
    for (int i = 0; i < 80; i++) {
        many->SlideX((float)i, (float)i + 1);
    }
    El* grown = many->Apply(Div(arena), StrL("many-effects"));
    utassertnear(grown->style.absLeft, 79);

    delete win;
    ArenaDelete(arena);
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

static void SourceNamedValueTransitionContractIsAvailable() {
    // This exercises the keyed transition path, not the host accessibility
    // preference. Reduced motion itself is covered by the pure state test.
    MotionSetReduced(false);
    motion::Transition linear =
        motion::Transition::New(100).Delay(50).Ease(EaseLinear);
    utassertnear(linear.durationMs, 100.f);
    utassertnear(linear.delayMs, 50.f);
    utassertnear(MotionSample(linear, 0.25f), 0.25f);

    motion::TransitionId fill(StrL("terms"), StrL("fill"));
    motion::TransitionId mark(StrL("terms"), StrL("mark-opacity"));
    utassert(fill != mark);
    utassert(fill == motion::TransitionId(StrL("terms"), StrL("fill")));

    utassertnear(motion::Interpolate<float>::Between(2.f, 10.f, 0.25f),
                 4.f);

    App app;
    Window* win = new Window();
    Arena* arena = ArenaNew();
    win->app = &app;
    win->frameNow = 1.0;
    Ctx cx = {&app, win, arena, {}};
    motion::TransitionId value(StrL("source-transition"));
    utassertnear(motion::transition(&cx, value, 0.f, linear), 0.f);
    utassertnear(motion::transition(&cx, value, 10.f, linear), 0.f);
    win->frameNow = 1.1;
    utassertnear(motion::transition(&cx, value, 10.f, linear), 5.f);

    delete win;
    ArenaDelete(arena);
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

// ─── springs ──────────────────────────────────────────────────────────────

// Run a spring forward in 16 ms frames, answering where it ends up.
static float SpringRun(SpringState* st, float target, const Spring& s,
                       double* now, int frames) {
    float v = 0;
    for (int i = 0; i < frames; i++) {
        *now += 0.016;
        v = SpringAdvance(st, target, s, *now, false).value;
    }
    return v;
}

static void ASpringArrivesAndStops() {
    Spring s = SpringNew(200);
    s.epsilon = 0.001f;
    SpringState st;
    double now = 0;
    // The first target is adopted outright, as a transition's is.
    SpringStep first = SpringAdvance(&st, 0.f, s, now, false);
    utassert(first.value == 0.f && !first.running);

    // It travels, without passing the target: critically damped.
    float mid = SpringRun(&st, 1.f, s, &now, 6);
    utassert(mid > 0.f && mid < 1.f);
    float v = SpringRun(&st, 1.f, s, &now, 60);
    utassertnear(v, 1.f);
    // And once it is there it asks for nothing more.
    now += 0.016;
    utassert(!SpringAdvance(&st, 1.f, s, now, false).running);
}

// The whole point of a spring: a target that changes mid-flight is turned
// around rather than restarted, so the value keeps going the way it was for
// a moment before it comes back.
static void ASpringCarriesItsVelocityThroughAReversal() {
    Spring s = SpringNew(200);
    SpringState st;
    double now = 0;
    SpringAdvance(&st, 0.f, s, now, false);
    SpringRun(&st, 1.f, s, &now, 5);
    float atTurn = st.position;
    utassert(atTurn > 0.f && atTurn < 1.f);
    utassert(st.velocity > 0.f);

    // Back to 0 from here. The next frame is still moving the old way.
    now += 0.016;
    float after = SpringAdvance(&st, 0.f, s, now, false).value;
    utassert(after > atTurn);
    utassert(st.velocity > 0.f);
    // It decelerates, turns, and gets back.
    float back = SpringRun(&st, 0.f, s, &now, 60);
    utassertnear(back, 0.f);
}

static void ASpringUnderOneOvershoots() {
    Spring s = SpringNew(200);
    s.damping = 0.5f;
    SpringState st;
    double now = 0;
    SpringAdvance(&st, 0.f, s, now, false);
    float peak = 0;
    for (int i = 0; i < 30; i++) {
        now += 0.016;
        float v = SpringAdvance(&st, 1.f, s, now, false).value;
        if (v > peak) {
            peak = v;
        }
    }
    utassert(peak > 1.f);
    // Critically damped, the same run never passes it.
    Spring critical = SpringNew(200);
    SpringState st2;
    double now2 = 0;
    SpringAdvance(&st2, 0.f, critical, now2, false);
    for (int i = 0; i < 30; i++) {
        now2 += 0.016;
        utassert(SpringAdvance(&st2, 1.f, critical, now2, false).value <= 1.f);
    }
}

static void ASuspendedSpringPinsItselfToTheTarget() {
    Spring s = SpringNew(200);
    SpringState st;
    double now = 0;
    SpringAdvance(&st, 0.f, s, now, false);
    SpringRun(&st, 1.f, s, &now, 3);
    utassert(st.position < 1.f);
    // travel(false) is a value the pointer is already moving: it takes the
    // target on the spot and keeps its state there, so travel resumes from
    // where the drag left it.
    s.travel = false;
    now += 0.016;
    SpringStep step = SpringAdvance(&st, 5.f, s, now, false);
    utassert(step.value == 5.f && !step.running);
    utassert(st.position == 5.f && st.velocity == 0.f);

    // Reduced motion does the same, whatever the travel says.
    s.travel = true;
    now += 0.016;
    utassert(SpringAdvance(&st, 9.f, s, now, true).value == 9.f);
    utassert(st.position == 9.f);
    // A response of zero is the degenerate spring: infinitely stiff.
    Spring instant = SpringNew(0);
    SpringState st3;
    double now3 = 0;
    SpringAdvance(&st3, 0.f, instant, now3, false);
    now3 += 0.016;
    utassert(SpringAdvance(&st3, 1.f, instant, now3, false).value == 1.f);
}

static void ACoarseEpsilonSettlesSooner() {
    Spring fine = SpringNew(200);
    Spring coarse = SpringNew(200);
    coarse.epsilon = 1.f;
    SpringState a;
    SpringState b;
    double now = 0;
    SpringAdvance(&a, 0.f, fine, now, false);
    SpringAdvance(&b, 0.f, coarse, now, false);
    int fineFrames = 0;
    int coarseFrames = 0;
    for (int i = 0; i < 200; i++) {
        now += 0.016;
        if (SpringAdvance(&a, 100.f, fine, now, false).running) {
            fineFrames++;
        }
        if (SpringAdvance(&b, 100.f, coarse, now, false).running) {
            coarseFrames++;
        }
    }
    // Both arrive; the coarse one stops asking for frames first.
    utassertnear(a.position, 100.f);
    utassertnear(b.position, 100.f);
    utassert(coarseFrames < fineFrames);
}

void TestMotion() {
    TestSuite("motion");
    TheEasingsAreTheCurvesRustNames();
    TheLoopingEasingsAreGpuisOwn();
    TheBezierRunsFromNothingToEverything();
    TheBezierMatchesWhatAnEngineSays();
    ThirdsMapTimeIdentically();
    TheLerpsAreTheThreeRustImplements();
    EffectTransitionAppliesEveryUpstreamEffect();
    ProgressWaitsOutTheDelayAndStopsAtTheEnd();
    AZeroDurationTargetChangeIsImmediate();
    AChangedTargetTransitionsOverTime();
    ReversingCarriesOnFromWhereItGotTo();
    ADelayHoldsThePreviousValue();
    AFinishedTransitionStopsAskingForFrames();
    ReducedMotionTakesTheTargetOutright();
    TheSameRuleCarriesAPointAndAColor();
    AChannelKeepsTwoValuesOfOneElementApart();
    SourceNamedValueTransitionContractIsAvailable();
    OpacityMultipliesEveryColourItPaints();
    ASpringArrivesAndStops();
    ASpringCarriesItsVelocityThroughAReversal();
    ASpringUnderOneOvershoots();
    ASuspendedSpringPinsItselfToTheTarget();
    ACoarseEpsilonSettlesSooner();
}
