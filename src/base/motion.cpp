#include "base/motion.h"
#include "gpui/platform.h"

namespace gpui {

uint32_t MotionId(Str id) {
    return (uint32_t)HashClickId(id);
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

} // namespace gpui
