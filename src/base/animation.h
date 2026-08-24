/* Easing and interpolation — crates/base/src/animation.rs

   The two halves of Rust's module that cross: the easing curves, and the Lerp
   trait's three implementations. What does not cross is `EffectTransition`,
   which hands a GPUI `AnimationElement` a closure that restyles the element on
   every frame — an element here carries no closures, and the frame tree is
   rebuilt from scratch anyway, so a caller reads the value it wants from
   `motion.h` and spells the effect itself. */

#include "gpui/gpui.h"

namespace gpui {

// cubic_bezier: Rust answers a closure over the four control points, so a
// caller names the curve once and calls it with t. There are no closures here,
// so the points travel with the sample.
//
// This is CSS's `cubic-bezier`: `x(s) = t` is solved for the curve parameter
// first — Newton, then bisection where the slope is no use — and `y(s)` is the
// answer. Reading `t` as the parameter and taking `y` off it directly, which
// is what this did before gpui-component 5b3e18d1, makes every curve run
// slower than the same control points do in a browser.
float CubicBezier(float x1, float y1, float x2, float y2, float t);

// An easing curve. Rust takes `impl Fn(f32) -> f32`; the same thing here is a
// function pointer, which is what lets a Motion be a POD.
using EaseFn = float (*)(float);

// The identity, which is what Rust's tests ease with.
float EaseLinear(float t);
// Fast start, slow end. What a Motion defaults to, and what an entrance wants.
float EaseOutCubic(float t);
// Slow start, fast end. For an exit.
float EaseInCubic(float t);
// Slow at both ends. For something moving from one place to another.
float EaseInOutCubic(float t);

// GPUI's own easings, from crates/gpui/src/elements/animation.rs, which are
// what the components that loop are written against rather than the cubics
// above. Rust does not clamp these — the doc comment says so, to leave room
// for a curve that overshoots — so neither do these.
float EaseQuadratic(float t);
// gpui's ease_in_out: the quadratic pair, not the cubic one above it.
float EaseInOutQuad(float t);
float EaseOutQuint(float t);
// bounce(easing): the curve forwards over the first half and backwards over
// the second, which is what turns a loop into a pulse.
float EaseBounce(EaseFn e, float t);
// bounce(ease_in_out), which is the skeleton's.
float EaseBounceInOut(float t);

// 0..1, which is what an easing's parameter has to be. Rust spells it
// `.clamp(0., 1.)` at each site.
float ClampF01(float t);

// The Lerp trait. Rust implements it for f32, Pixels, Point<Pixels> and Hsla;
// Pixels is a float here, and the two aggregates are these overloads.
float Lerp(float a, float b, float t);
Point Lerp(Point a, Point b, float t);
// Rust interpolates the four HSLA channels, and says why: it is meant for
// transitions between near-grayscale interface colors, where the hue is not
// doing any work. This walks the same four, converting either side of the
// step through gpui::Hsla; the ends come back as they were given, since eight
// bits a channel cannot promise a round trip lands on the byte it started on.
Rgba Lerp(Rgba a, Rgba b, float t);

} // namespace gpui
