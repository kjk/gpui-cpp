/* Value transitions — crates/base/src/motion.rs

   A CSS-like timing policy for a value the caller owns. The caller asks for
   the value it wants; this answers the value to draw *now*, on its way there.
   It never picks a visual property — that is what makes it different from
   animation.rs's EffectTransition, which restyles an element for you.

   The state is window-keyed, the way Rust's `window.use_keyed_state` is: an
   element tree that is rebuilt every frame has nowhere else to keep where a
   value had got to. A transition still running asks for another frame, which
   is `window.request_animation_frame()`.

   Components opt in explicitly; nothing here installs motion by default. */

#include "base/animation.h"

namespace gpui {

// motion::Transition: how long, how late, and along which curve. Rust builds
// it with a chain; the fields are the chain.
struct Motion {
    float durationMs = 0;
    float delayMs = 0;
    EaseFn ease = EaseOutCubic;
};

inline Motion MotionNew(float durationMs) {
    Motion m;
    m.durationMs = durationMs;
    return m;
}

// One independently transitioning value. Rust's TransitionId is an ElementId
// with a channel name under it, so one element can transition several values
// without their state colliding; the same pair hashed is the key here.
uint32_t MotionId(Str id);
uint32_t MotionId(Str id, Str channel);

// Transition::progress: nothing has happened yet while the delay runs, a
// duration of zero is over as soon as it starts, and the rest is the fraction
// of the duration that has passed, capped at 1.
float MotionProgress(const Motion& m, float elapsedMs);
// Transition::sample: the curve at that fraction.
float MotionSample(const Motion& m, float progress);

// cx.reduce_motion(). Rust reads the platform's setting; so does this, on the
// two platforms that have one to read (Windows' client-area animation and
// macOS' reduce-motion switch). A value that is transitioning when it goes on
// adopts its target on the next frame rather than finishing the curve.
bool MotionReduced();
// For a caller that wants to decide for itself — the story's settings menu,
// and the tests.
void MotionSetReduced(bool on);

// Where a value has got to. Rust keeps `from`, `target` and `started_at`;
// `init` stands in for the state having been created by use_keyed_state's
// closure, since a keyed slot here arrives zeroed.
template <typename T>
struct MotionState {
    T from = {};
    T target = {};
    double startedAt = 0;
    bool init = false;
};

template <typename T>
struct MotionStep {
    T value = {};
    // Whether another frame is wanted: request_animation_frame is called for
    // exactly this.
    bool running = false;
};

// Two values are the same one. Rust bounds T on PartialEq; these are the
// overloads that stand for it.
inline bool MotionEq(float a, float b) {
    return a == b;
}
inline bool MotionEq(Point a, Point b) {
    return a.x == b.x && a.y == b.y;
}
inline bool MotionEq(Rgba a, Rgba b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

// The whole rule, with the state and the clock passed in — which is what makes
// it testable without a window. `transition()` in Rust, minus the two lines
// that fetch the state and ask for a frame.
//
// The first target is adopted outright; a later change transitions from the
// value sampled at that instant, so reversing halfway does not jump.
template <typename T>
MotionStep<T> MotionAdvance(MotionState<T>* st, T target, const Motion& m,
                            double now, bool reduced) {
    MotionStep<T> out;
    if (!st->init) {
        st->init = true;
        st->from = target;
        st->target = target;
        st->startedAt = now;
    }
    if (reduced || m.durationMs <= 0) {
        st->from = target;
        st->target = target;
        st->startedAt = now;
        out.value = target;
        return out;
    }
    float elapsedMs = (float)((now - st->startedAt) * 1000.0);
    float progress = MotionProgress(m, elapsedMs);
    T sampled = Lerp(st->from, st->target, MotionSample(m, progress));
    if (!MotionEq(st->target, target)) {
        // The target moved: carry on from where the last one had got to.
        st->from = sampled;
        st->target = target;
        st->startedAt = now;
        out.value = sampled;
        out.running = true;
        return out;
    }
    out.value = sampled;
    out.running = progress < 1.f && !MotionEq(st->from, st->target);
    return out;
}

// The clock a frame runs on: one instant for the whole frame, so every
// transition in it samples the same `now`. Rust reads the executor's clock,
// which does not move inside a frame either.
double MotionNow(Ctx* cx);
// The keyed slot behind one id, and the frame it asks for. Split out so the
// template below is the only generic part.
void* MotionSlot(Ctx* cx, uint32_t key, int size);
void MotionWantsFrame(Ctx* cx);

// Animation::repeat: a loop with no target and no end. The phase of a cycle
// of `periodMs`, put through `ease` — the delta GPUI hands the closure of a
// `with_animation(.., Animation::new(d).repeat(), ..)`. Something showing one
// is asking for a frame every frame, which is what a spinner is.
//
// Rust's repeats do not consult `reduce_motion`, and neither does this: a
// spinner that stopped spinning would be saying the wrong thing.
float MotionRepeat(Ctx* cx, uint32_t key, float periodMs,
                   EaseFn ease = nullptr);

// motion::transition: the value to draw now, on its way to `target`.
template <typename T>
T MotionValue(Ctx* cx, uint32_t key, T target, const Motion& m) {
    auto* st =
        (MotionState<T>*)MotionSlot(cx, key, (int)sizeof(MotionState<T>));
    if (!st) {
        return target;
    }
    MotionStep<T> step =
        MotionAdvance(st, target, m, MotionNow(cx), MotionReduced());
    if (step.running) {
        MotionWantsFrame(cx);
    }
    return step.value;
}

} // namespace gpui
