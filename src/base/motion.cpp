#include "base/motion.h"
#include "gpui/platform.h"

namespace gpui {

uint32_t MotionId(Str id) {
    return (uint32_t)HashClickId(id);
}

uint32_t MotionName(Ctx* cx, Str name) {
    return KeyedName(cx, name);
}

uint32_t MotionId(Str id, Str channel) {
    // ElementId::NamedChild(id, channel): the channel is part of the key, so
    // one element can transition its fill and its offset separately.
    return (uint32_t)HashClickId(id) * 31u + (uint32_t)HashClickId(channel);
}

float MotionProgress(const Motion& m, float elapsedMs) {
    if (elapsedMs <= m.delayMs) {
        return 0.f;
    }
    if (m.durationMs <= 0) {
        return 1.f;
    }
    float p = (elapsedMs - m.delayMs) / m.durationMs;
    return p < 1.f ? p : 1.f;
}

float MotionSample(const Motion& m, float progress) {
    float p = progress < 0.f ? 0.f : (progress > 1.f ? 1.f : progress);
    EaseFn ease = m.ease ? m.ease : EaseOutCubic;
    return ease(p);
}

static bool gReducedAsked = false;
static bool gReduced = false;

bool MotionReduced() {
    if (!gReducedAsked) {
        gReducedAsked = true;
        gReduced = PlatReduceMotion();
    }
    return gReduced;
}

void MotionSetReduced(bool on) {
    gReducedAsked = true;
    gReduced = on;
}

// The clock a loop or a one-shot runs on: when it started, and nothing else —
// neither has a target to leave from.
struct MotionLoopState {
    double startedAt = 0;
    bool init = false;
};

// `with_animation`, which is not `motion::transition`: Rust gates
// `reduce_motion` inside `motion::transition` alone (motion.rs, the one
// `cx.reduce_motion()` in the crate), and every `with_animation` — the
// dialog's slide-down and fade, the sheet's slide — plays whatever the
// desktop's animation setting says. This had checked it, so on a machine
// with animation effects off (SPI_GETCLIENTAREAANIMATION false, which is
// not rare) a dialog appeared in place, fully opaque, with nothing having
// moved. `MotionTransition` keeps the check, where Rust has it.
float MotionAppear(Ctx* cx, uint32_t key, float durationMs, EaseFn ease) {
    if (durationMs <= 0) {
        return 1.f;
    }
    auto* st =
        (MotionLoopState*)MotionSlot(cx, key, (int)sizeof(MotionLoopState));
    if (!st) {
        return 1.f;
    }
    double now = MotionNow(cx);
    if (!st->init) {
        st->init = true;
        st->startedAt = now;
    }
    float elapsedMs = (float)((now - st->startedAt) * 1000.0);
    float t = elapsedMs / durationMs;
    if (t >= 1.f) {
        return 1.f;
    }
    if (t < 0.f) {
        t = 0.f;
    }
    MotionWantsFrame(cx);
    return ease ? ease(t) : t;
}

float MotionRepeat(Ctx* cx, uint32_t key, float periodMs, EaseFn ease) {
    if (periodMs <= 0) {
        return 0.f;
    }
    auto* st =
        (MotionLoopState*)MotionSlot(cx, key, (int)sizeof(MotionLoopState));
    if (!st) {
        return 0.f;
    }
    double now = MotionNow(cx);
    if (!st->init) {
        st->init = true;
        st->startedAt = now;
    }
    float elapsedMs = (float)((now - st->startedAt) * 1000.0);
    float phase = elapsedMs / periodMs;
    // The whole turns come off rather than the phase growing without end,
    // which would lose its precision within a day of running.
    phase -= (float)(int)phase;
    if (phase < 0) {
        phase = 0;
    }
    MotionWantsFrame(cx);
    return ease ? ease(phase) : phase;
}

double MotionNow(Ctx* cx) {
    // The frame's own instant, which WindowDrawFrame stamps before it renders.
    if (cx && cx->win && cx->win->frameNow > 0) {
        return cx->win->frameNow;
    }
    return TimeNow();
}

void* MotionSlot(Ctx* cx, uint32_t key, int size) {
    return cx ? WindowMotionState(cx->win, key, size) : nullptr;
}

void MotionWantsFrame(Ctx* cx) {
    if (cx) {
        WindowRequestAnimationFrame(cx->win);
    }
}

// ─── springs ──────────────────────────────────────────────────────────────

// The damped harmonic oscillator, stepped exactly rather than integrated: at
// these frame times an Euler step visibly changes the motion with the frame
// rate, and the closed form does not. Mass is 1, so the stiffness is w0 * w0
// and the damping coefficient 2 * zeta * w0.
//
// x is the displacement from the target, v the velocity. Each case is the
// standard solution of x'' + 2*zeta*w0*x' + w0^2*x = 0 for the initial
// conditions (x, v).
static void SpringStepExact(float w0, float zeta, float dt, float* x,
                            float* v) {
    float x0 = *x;
    float v0 = *v;
    if (dt <= 0 || w0 <= 0) {
        return;
    }
    if (zeta < 1.f - 1e-4f) {
        // Underdamped: it passes the target and comes back.
        float wd = w0 * sqrtf(1.f - zeta * zeta);
        float e = expf(-zeta * w0 * dt);
        float c1 = x0;
        float c2 = (v0 + zeta * w0 * x0) / wd;
        float cosd = cosf(wd * dt);
        float sind = sinf(wd * dt);
        *x = e * (c1 * cosd + c2 * sind);
        *v = e * ((c2 * wd - zeta * w0 * c1) * cosd -
                  (c1 * wd + zeta * w0 * c2) * sind);
        return;
    }
    if (zeta > 1.f + 1e-4f) {
        // Overdamped: two real rates, and it crawls in on the slower one.
        float r = w0 * sqrtf(zeta * zeta - 1.f);
        float r1 = -zeta * w0 + r;
        float r2 = -zeta * w0 - r;
        float c2 = (v0 - r1 * x0) / (r2 - r1);
        float c1 = x0 - c2;
        float e1 = expf(r1 * dt);
        float e2 = expf(r2 * dt);
        *x = c1 * e1 + c2 * e2;
        *v = c1 * r1 * e1 + c2 * r2 * e2;
        return;
    }
    // Critically damped, which is the default: the fastest approach that
    // never passes the target.
    float e = expf(-w0 * dt);
    float c = v0 + w0 * x0;
    *x = (x0 + c * dt) * e;
    *v = (v0 - c * w0 * dt) * e;
}

SpringStep SpringAdvance(SpringState* st, float target, const Spring& s,
                         double now, bool reduced) {
    SpringStep out;
    out.value = target;
    if (!st->init) {
        st->init = true;
        st->position = target;
        st->velocity = 0;
        st->target = target;
        st->updatedAt = now;
        return out;
    }
    // The common case by far: a spring nothing is moving. It has no state to
    // advance and no frame to ask for.
    if (st->position == target && st->velocity == 0) {
        st->target = target;
        st->updatedAt = now;
        return out;
    }
    if (reduced || !s.travel || s.responseMs <= 0) {
        st->position = target;
        st->velocity = 0;
        st->target = target;
        st->updatedAt = now;
        return out;
    }
    // Over the frame that just elapsed, which the *previous* target governed,
    // before adopting the new one for the frame to come. That is what carries
    // the velocity through a reversal.
    float dt = (float)(now - st->updatedAt);
    if (dt < 0) {
        dt = 0;
    }
    // A tab out and back is not a frame's worth of motion to catch up on.
    if (dt > 0.1f) {
        dt = 0.1f;
    }
    float w0 = 6.2831853f / (s.responseMs / 1000.f);
    float x = st->position - st->target;
    float v = st->velocity;
    SpringStepExact(w0, s.damping, dt, &x, &v);
    float position = st->target + x;
    st->target = target;
    st->updatedAt = now;
    // Settled: within epsilon of the target, and slow enough that what is
    // left to travel is inside it too — a velocity in units per second is
    // read as the distance one response period of it would cover.
    float rest = position - target;
    float restAbs = rest < 0 ? -rest : rest;
    float speed = v < 0 ? -v : v;
    if (restAbs <= s.epsilon && speed * (s.responseMs / 1000.f) <= s.epsilon) {
        st->position = target;
        st->velocity = 0;
        out.value = target;
        return out;
    }
    st->position = position;
    st->velocity = v;
    out.value = position;
    out.running = true;
    return out;
}

float SpringValue(Ctx* cx, uint32_t key, float target, const Spring& s) {
    auto* st = (SpringState*)MotionSlot(cx, key, (int)sizeof(SpringState));
    if (!st) {
        return target;
    }
    SpringStep step =
        SpringAdvance(st, target, s, MotionNow(cx), MotionReduced());
    if (step.running) {
        MotionWantsFrame(cx);
    }
    return step.value;
}

} // namespace gpui
