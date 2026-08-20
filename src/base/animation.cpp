#include "base/animation.h"

namespace gpui {

// The same three-line clamp scrollbar.cpp and slider.cpp each keep: base has
// no float clamp of its own, and one header for one expression is worse.
static float ClampF(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

float CubicBezier(float x1, float y1, float x2, float y2, float t) {
    float oneT = 1.f - t;
    float oneT2 = oneT * oneT;
    float t2 = t * t;
    float t3 = t2 * t;
    // The x half is worked out and dropped, exactly as Rust does — it is there
    // to say what the curve is, not to be solved.
    (void)(3.f * x1 * oneT2 * t + 3.f * x2 * oneT * t2 + t3);
    return 3.f * y1 * oneT2 * t + 3.f * y2 * oneT * t2 + t3;
}

float EaseLinear(float t) {
    return ClampF(t, 0.f, 1.f);
}

float EaseOutCubic(float t) {
    t = ClampF(t, 0.f, 1.f);
    float u = 1.f - t;
    return 1.f - u * u * u;
}

float EaseInCubic(float t) {
    t = ClampF(t, 0.f, 1.f);
    return t * t * t;
}

float EaseInOutCubic(float t) {
    t = ClampF(t, 0.f, 1.f);
    if (t < 0.5f) {
        return 4.f * t * t * t;
    }
    float u = -2.f * t + 2.f;
    return 1.f - (u * u * u) / 2.f;
}

float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

Point Lerp(Point a, Point b, float t) {
    return {Lerp(a.x, b.x, t), Lerp(a.y, b.y, t)};
}

// A channel is a byte, so the result is rounded rather than truncated: a
// half-step that always rounded down would never reach the target.
static uint8_t LerpByte(uint8_t a, uint8_t b, float t) {
    float v = Lerp((float)a, (float)b, t);
    return (uint8_t)ClampF(v + 0.5f, 0.f, 255.f);
}

Rgba Lerp(Rgba a, Rgba b, float t) {
    return Rgba{LerpByte(a.r, b.r, t), LerpByte(a.g, b.g, t),
                LerpByte(a.b, b.b, t), LerpByte(a.a, b.a, t)};
}

} // namespace gpui
