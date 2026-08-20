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

double MotionNow(Ctx* cx) {
    // The frame's own instant, which WindowDrawFrame stamps before it renders.
    if (cx && cx->win && cx->win->frameNow > 0) {
        return cx->win->frameNow;
    }
    return TimeNow();
}

void* MotionSlot(Ctx* cx, uint32_t key, int size) {
    return cx ? WindowKeyedState(cx->win, key, size, nullptr) : nullptr;
}

void MotionWantsFrame(Ctx* cx) {
    if (cx) {
        WindowRequestAnimationFrame(cx->win);
    }
}

} // namespace gpui
