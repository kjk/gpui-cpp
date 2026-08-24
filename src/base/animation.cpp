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

float ClampF01(float t) {
    return ClampF(t, 0.f, 1.f);
}

float EaseQuadratic(float t) {
    return t * t;
}

float EaseInOutQuad(float t) {
    if (t < 0.5f) {
        return 2.f * t * t;
    }
    float x = -2.f * t + 2.f;
    return 1.f - x * x / 2.f;
}

float EaseOutQuint(float t) {
    float u = 1.f - t;
    return 1.f - u * u * u * u * u;
}

float EaseBounce(EaseFn e, float t) {
    if (!e) {
        e = EaseLinear;
    }
    return t < 0.5f ? e(t * 2.f) : e((1.f - t) * 2.f);
}

float EaseBounceInOut(float t) {
    return EaseBounce(EaseInOutQuad, t);
}

float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

Point Lerp(Point a, Point b, float t) {
    return {Lerp(a.x, b.x, t), Lerp(a.y, b.y, t)};
}

// `impl Lerp for Hsla`: the four channels interpolated straight, hue
// included. Rust's colour is an Hsla and stays one; ours is four bytes, so
// this converts either side, walks in HSL and converts back — which is the
// path Rust takes and not the one through RGB.
//
// The ends are handed back as they came in rather than through the round
// trip. Rust's `a + (b - a) * t` returns the endpoint's own floats at 0 and
// 1; eight bits a channel cannot promise that, and a transition that settled
// a byte away from its target would stay there.
Rgba Lerp(Rgba a, Rgba b, float t) {
    if (t <= 0.f) {
        return a;
    }
    if (t >= 1.f) {
        return b;
    }
    Hsla x = HslaFromRgba(a);
    Hsla y = HslaFromRgba(b);
    return HslaToRgba(Hsla{Lerp(x.h, y.h, t), Lerp(x.s, y.s, t),
                           Lerp(x.l, y.l, t), Lerp(x.a, y.a, t)});
}

} // namespace gpui
