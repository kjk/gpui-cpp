#ifndef GPUI_BASE_MOTION_H_
#define GPUI_BASE_MOTION_H_
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

// motion.rs has a Transition too. Keep it in the source module's namespace so
// it can coexist with animation.rs::Transition, rather than renaming one of
// the two public Rust contracts in the C++ surface.
namespace motion {

// A value that can be interpolated between two application-owned targets.
// Rust expresses this as a trait with a blanket implementation for Lerp; the
// C++ specialization seam has the same shape and the default delegates to the
// corresponding Lerp overload.
template <typename T>
struct Interpolate {
    static T Between(const T& from, const T& target, float progress) {
        return Lerp(from, target, progress);
    }
};

// CSS-like timing policy: how long, how late, and along which curve. Rust
// builds it with a consuming chain; this POD returns a copy from the same
// operations so named policies can be composed without retained ownership.
struct Transition {
    float durationMs = 0;
    float delayMs = 0;
    EaseFn ease = EaseOutCubic;

    static Transition New(float durationMs) {
        Transition policy;
        policy.durationMs = durationMs;
        return policy;
    }

    Transition Delay(float ms) const {
        Transition policy = *this;
        policy.delayMs = ms;
        return policy;
    }

    Transition Ease(EaseFn fn) const {
        Transition policy = *this;
        policy.ease = fn;
        return policy;
    }
};

// One independently transitioning value. Upstream wraps ElementId and adds a
// named child for the retained transition state. The runtime's keyed store is
// already the folded GlobalElementId, so this wrapper carries that POD key.
struct TransitionId {
    uint32_t key = 0;

    TransitionId() = default;
    explicit TransitionId(uint32_t value) : key(value) {}
    explicit TransitionId(Str id);
    TransitionId(Str id, Str channel);

    bool operator==(const TransitionId& other) const {
        return key == other.key;
    }
    bool operator!=(const TransitionId& other) const {
        return key != other.key;
    }
};

} // namespace motion

// Compatibility spelling used by the port before the Rust module namespace
// was restored. It is the same policy, not an adapter state.
using Motion = motion::Transition;

inline Motion MotionNew(float durationMs) {
    return motion::Transition::New(durationMs);
}

// One independently transitioning value. Rust's TransitionId is an ElementId
// with a channel name under it, so one element can transition several values
// without their state colliding; the same pair hashed is the key here.
uint32_t MotionId(Str id);
uint32_t MotionId(Str id, Str channel);
// `window.use_keyed_state(id, ..)` in motion.rs is keyed by the whole id
// stack, so a transition named among its siblings is still its own. This is
// that name folded into the stack the widget is being built under.
uint32_t MotionName(Ctx* cx, Str name);

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
    T sampled = motion::Interpolate<T>::Between(
        st->from, st->target, MotionSample(m, progress));
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

// with_animation, the one-shot kind: how far into a `durationMs` run the
// element is, from the frame it first appeared in. GPUI keeps that clock in
// the element's own state and drops it with the element, which is why the
// slots here are swept — something that goes away and comes back plays its
// entrance again.
//
// Under reduced motion an entrance is over before it starts, which is the
// same answer `transition` gives.
float MotionAppear(Ctx* cx, uint32_t key, float durationMs,
                   EaseFn ease = nullptr);

// Where a transition starts from, for a value whose first frame is not where
// it is going. `transition()` has no such call — Rust's animated placeholder
// keeps its own `from` and an epoch beside it and restarts the run by hand —
// and the same effect here is to write the state before the first ask: the
// next MotionValue sees a target that differs from this one and runs the
// curve from it.
template <typename T>
void MotionSeed(Ctx* cx, uint32_t key, T from) {
    auto* st =
        (MotionState<T>*)MotionSlot(cx, key, (int)sizeof(MotionState<T>));
    if (!st) {
        return;
    }
    st->init = true;
    st->from = from;
    st->target = from;
    st->startedAt = MotionNow(cx);
}

// ─── springs ──────────────────────────────────────────────────────────────
//
// motion::Spring. A transition cannot carry *velocity* across a change of
// target: it restarts its curve from the value sampled at that instant, which
// is continuous in position and not in speed, so a value reversed mid-flight
// jumps to the new curve's initial pace. A spring turns around instead.
//
// The rule upstream applies, and this follows: spring where the target
// changes faster than the motion finishes — a switch toggled twice, a tab
// indicator chasing the pointer, a dock being dragged — and transition where
// a target is set once and runs to its end.
struct Spring {
    // The period one full undamped oscillation would take, which is the scale
    // the motion is felt at rather than the moment it stops. A spring has no
    // end to schedule: it settles once it is within `epsilon` of its target.
    // Zero adopts the target on the spot, as a zero duration does.
    float responseMs = 0;
    // The damping ratio. 1 is critical — it never passes its target — below
    // that it overshoots and comes back, above it crawls in. A value bounded
    // by the geometry around it (a thumb inside a track) keeps 1.
    float damping = 1.f;
    // How close counts as arrived, in the target's own units. The default
    // suits a 0..1 value; a spring over pixels settles perceptibly sooner
    // with a coarser one, and stops asking for frames it has nothing to draw
    // with.
    float epsilon = 0.001f;
    // Whether the spring travels at all. A value the pointer is already
    // moving — a panel being dragged by its handle — must not lag behind it,
    // so travel is off for as long as the drag lasts; the state stays pinned
    // to the target meanwhile, so travel resumes from where the drag left it
    // rather than from where the spring had got to before it started.
    bool travel = true;
};

inline Spring SpringNew(float responseMs) {
    Spring s;
    s.responseMs = responseMs;
    return s;
}

// Where a sprung value is and how fast it is going. `target` is what it was
// travelling to over the frame that just elapsed; a new one is adopted for
// the frame to come, keeping both.
struct SpringState {
    float position = 0;
    float velocity = 0;
    float target = 0;
    double updatedAt = 0;
    bool init = false;
};

struct SpringStep {
    float value = 0;
    bool running = false;
};

// The whole rule with the state and the clock passed in, which is what makes
// it testable without a window.
SpringStep SpringAdvance(SpringState* st, float target, const Spring& s,
                         double now, bool reduced);

// motion::spring: the value to draw now, on its way to `target`.
float SpringValue(Ctx* cx, uint32_t key, float target, const Spring& s);

// MotionSeed's counterpart: where a spring starts from, for a value whose
// first frame is not where it is going.
inline void SpringSeed(Ctx* cx, uint32_t key, float from) {
    auto* st = (SpringState*)MotionSlot(cx, key, (int)sizeof(SpringState));
    if (!st) {
        return;
    }
    st->init = true;
    st->position = from;
    st->velocity = 0;
    st->target = from;
    st->updatedAt = MotionNow(cx);
}

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

namespace motion {

// Source-named entry point. Fetching Window separately is unnecessary here:
// Ctx carries the render window and app together, and MotionValue performs the
// same keyed-state lookup and animation-frame request as Rust's transition.
template <typename T>
T transition(Ctx* cx, TransitionId id, T target, const Transition& policy) {
    return MotionValue(cx, id.key, target, policy);
}

// Spring lives at gpui scope for existing callers; make the Rust module path
// available as well without duplicating policy or state.
using Spring = gpui::Spring;
using SpringState = gpui::SpringState;
using SpringStep = gpui::SpringStep;

inline float spring(Ctx* cx, TransitionId id, float target,
                    const Spring& policy) {
    return SpringValue(cx, id.key, target, policy);
}

} // namespace motion

} // namespace gpui
#endif // GPUI_BASE_MOTION_H_
