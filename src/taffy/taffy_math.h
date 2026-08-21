/* Numerical helpers for values that may not be defined —
 * taffy/src/util/math.rs.
 *
 * Rust states the rule once, as the `MaybeMath<In, Out>` trait: if the
 * left-hand value is None the result is None, and a None right-hand value is
 * skipped. With no traits, each pairing taffy actually uses is an overload.
 *
 * There used to be three overloads of each — `Optf op Optf`, `Optf op float`
 * and `float op Optf` — because `Option<f32>` was a distinct struct. An
 * `Optf` is now a `float` (geometry.h), so a plain float is an `Optf` that
 * happens to be Some and the three collapse into one. The rule reads the same
 * either way, which is why they collapse rather than conflict: the None-left
 * arm never fires for a value that cannot be None.
 */

#ifndef GPUI_TAFFY_MATH_H_
#define GPUI_TAFFY_MATH_H_

#include "taffy/style.h"

namespace taffy {

// ─── Optf op Optf ────────────────────────────────────────────────────────

inline Optf MaybeMin(Optf a, Optf b) {
    if (IsNone(a)) {
        return None();
    }
    return IsSome(b) ? F32Min(a, b) : a;
}

inline Optf MaybeMax(Optf a, Optf b) {
    if (IsNone(a)) {
        return None();
    }
    return IsSome(b) ? F32Max(a, b) : a;
}

inline Optf MaybeAdd(Optf a, Optf b) {
    if (IsNone(a)) {
        return None();
    }
    return IsSome(b) ? a + b : a;
}

inline Optf MaybeSub(Optf a, Optf b) {
    if (IsNone(a)) {
        return None();
    }
    return IsSome(b) ? a - b : a;
}

inline Optf MaybeClamp(Optf a, Optf lo, Optf hi) {
    if (IsNone(a)) {
        return None();
    }
    float v = a;
    if (IsSome(hi)) {
        v = F32Min(v, hi);
    }
    if (IsSome(lo)) {
        v = F32Max(v, lo);
    }
    return v;
}

// ─── AvailableSpace op Optf ──────────────────────────────────────────────
//
// A min against a concrete value makes an indefinite constraint definite; a
// max, an add or a sub leaves it alone.

inline AvailableSpace MaybeMin(AvailableSpace a, Optf b) {
    if (a.kind == AvailableSpace::Kind::Definite) {
        return IsSome(b) ? AvailableSpace::Definite(F32Min(a.value, b)) : a;
    }
    return IsSome(b) ? AvailableSpace::Definite(b) : a;
}

inline AvailableSpace MaybeMax(AvailableSpace a, Optf b) {
    if (a.kind == AvailableSpace::Kind::Definite && IsSome(b)) {
        return AvailableSpace::Definite(F32Max(a.value, b));
    }
    return a;
}

inline AvailableSpace MaybeAdd(AvailableSpace a, Optf b) {
    if (a.kind == AvailableSpace::Kind::Definite && IsSome(b)) {
        return AvailableSpace::Definite(a.value + b);
    }
    return a;
}

inline AvailableSpace MaybeSub(AvailableSpace a, Optf b) {
    if (a.kind == AvailableSpace::Kind::Definite && IsSome(b)) {
        return AvailableSpace::Definite(a.value - b);
    }
    return a;
}

inline AvailableSpace MaybeClamp(AvailableSpace a, Optf lo, Optf hi) {
    if (a.kind != AvailableSpace::Kind::Definite) {
        return a;
    }
    float v = a.value;
    if (IsSome(hi)) {
        v = F32Min(v, hi);
    }
    if (IsSome(lo)) {
        v = F32Max(v, lo);
    }
    return AvailableSpace::Definite(v);
}

// ─── the same, component-wise over a size ────────────────────────────────

inline SizeFOpt MaybeMin(SizeFOpt a, SizeFOpt b) {
    return {MaybeMin(a.w, b.w), MaybeMin(a.h, b.h)};
}

inline SizeFOpt MaybeMax(SizeFOpt a, SizeFOpt b) {
    return {MaybeMax(a.w, b.w), MaybeMax(a.h, b.h)};
}

inline SizeFOpt MaybeAdd(SizeFOpt a, SizeFOpt b) {
    return {MaybeAdd(a.w, b.w), MaybeAdd(a.h, b.h)};
}

inline SizeFOpt MaybeSub(SizeFOpt a, SizeFOpt b) {
    return {MaybeSub(a.w, b.w), MaybeSub(a.h, b.h)};
}

inline SizeFOpt MaybeClamp(SizeFOpt a, SizeFOpt lo, SizeFOpt hi) {
    return {MaybeClamp(a.w, lo.w, hi.w), MaybeClamp(a.h, lo.h, hi.h)};
}

inline SizeAvail MaybeSub(SizeAvail a, SizeFOpt b) {
    return {MaybeSub(a.width, b.w), MaybeSub(a.height, b.h)};
}

inline SizeAvail MaybeClamp(SizeAvail a, SizeFOpt lo, SizeFOpt hi) {
    return {MaybeClamp(a.width, lo.w, hi.w), MaybeClamp(a.height, lo.h, hi.h)};
}

inline SizeAvail MaybeSet(SizeAvail a, SizeFOpt v) {
    return a.MaybeSet(v);
}

// Rust writes this as `Size::map(Some)` — a definite size seen as an
// everywhere-Some optional one. Nothing to do: it already is one.
constexpr SizeFOpt AsOptional(SizeF s) {
    return s;
}

} // namespace taffy

#endif // GPUI_TAFFY_MATH_H_
