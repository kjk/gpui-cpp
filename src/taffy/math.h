/* Numerical helpers for values that may not be defined —
 * taffy/src/util/math.rs.
 *
 * Rust states the rule once, as the `MaybeMath<In, Out>` trait: if the
 * left-hand value is None the result is None, and a None right-hand value is
 * skipped. With no traits, each pairing taffy actually uses is an overload.
 */

#ifndef GPUI_TAFFY_MATH_H_
#define GPUI_TAFFY_MATH_H_

#include "taffy/style.h"

namespace taffy {

// ─── Optf op Optf ────────────────────────────────────────────────────────

inline Optf MaybeMin(Optf a, Optf b) {
    if (!a.IsSome()) {
        return Optf();
    }
    return b.IsSome() ? Optf(F32Min(a.val, b.val)) : a;
}

inline Optf MaybeMax(Optf a, Optf b) {
    if (!a.IsSome()) {
        return Optf();
    }
    return b.IsSome() ? Optf(F32Max(a.val, b.val)) : a;
}

inline Optf MaybeAdd(Optf a, Optf b) {
    if (!a.IsSome()) {
        return Optf();
    }
    return b.IsSome() ? Optf(a.val + b.val) : a;
}

inline Optf MaybeSub(Optf a, Optf b) {
    if (!a.IsSome()) {
        return Optf();
    }
    return b.IsSome() ? Optf(a.val - b.val) : a;
}

inline Optf MaybeClamp(Optf a, Optf lo, Optf hi) {
    if (!a.IsSome()) {
        return Optf();
    }
    float v = a.val;
    if (hi.IsSome()) {
        v = F32Min(v, hi.val);
    }
    if (lo.IsSome()) {
        v = F32Max(v, lo.val);
    }
    return Optf(v);
}

// ─── Optf op float ───────────────────────────────────────────────────────

inline Optf MaybeMin(Optf a, float b) {
    return a.IsSome() ? Optf(F32Min(a.val, b)) : Optf();
}

inline Optf MaybeMax(Optf a, float b) {
    return a.IsSome() ? Optf(F32Max(a.val, b)) : Optf();
}

inline Optf MaybeAdd(Optf a, float b) {
    return a.IsSome() ? Optf(a.val + b) : Optf();
}

inline Optf MaybeSub(Optf a, float b) {
    return a.IsSome() ? Optf(a.val - b) : Optf();
}

inline Optf MaybeClamp(Optf a, float lo, float hi) {
    return a.IsSome() ? Optf(F32Max(F32Min(a.val, hi), lo)) : Optf();
}

// ─── float op Optf ───────────────────────────────────────────────────────

inline float MaybeMin(float a, Optf b) {
    return b.IsSome() ? F32Min(a, b.val) : a;
}

inline float MaybeMax(float a, Optf b) {
    return b.IsSome() ? F32Max(a, b.val) : a;
}

inline float MaybeAdd(float a, Optf b) {
    return b.IsSome() ? a + b.val : a;
}

inline float MaybeSub(float a, Optf b) {
    return b.IsSome() ? a - b.val : a;
}

inline float MaybeClamp(float a, Optf lo, Optf hi) {
    float v = a;
    if (hi.IsSome()) {
        v = F32Min(v, hi.val);
    }
    if (lo.IsSome()) {
        v = F32Max(v, lo.val);
    }
    return v;
}

// ─── AvailableSpace op float ─────────────────────────────────────────────
//
// A min against a concrete value makes an indefinite constraint definite; a
// max, an add or a sub leaves it alone.

inline AvailableSpace MaybeMin(AvailableSpace a, float b) {
    if (a.kind == AvailableSpace::Kind::Definite) {
        return AvailableSpace::Definite(F32Min(a.value, b));
    }
    return AvailableSpace::Definite(b);
}

inline AvailableSpace MaybeMax(AvailableSpace a, float b) {
    if (a.kind == AvailableSpace::Kind::Definite) {
        return AvailableSpace::Definite(F32Max(a.value, b));
    }
    return a;
}

inline AvailableSpace MaybeAdd(AvailableSpace a, float b) {
    if (a.kind == AvailableSpace::Kind::Definite) {
        return AvailableSpace::Definite(a.value + b);
    }
    return a;
}

inline AvailableSpace MaybeSub(AvailableSpace a, float b) {
    if (a.kind == AvailableSpace::Kind::Definite) {
        return AvailableSpace::Definite(a.value - b);
    }
    return a;
}

inline AvailableSpace MaybeClamp(AvailableSpace a, float lo, float hi) {
    if (a.kind == AvailableSpace::Kind::Definite) {
        return AvailableSpace::Definite(F32Max(F32Min(a.value, hi), lo));
    }
    return a;
}

// ─── AvailableSpace op Optf ──────────────────────────────────────────────

inline AvailableSpace MaybeMin(AvailableSpace a, Optf b) {
    if (a.kind == AvailableSpace::Kind::Definite) {
        return b.IsSome() ? AvailableSpace::Definite(F32Min(a.value, b.val))
                          : a;
    }
    return b.IsSome() ? AvailableSpace::Definite(b.val) : a;
}

inline AvailableSpace MaybeMax(AvailableSpace a, Optf b) {
    if (a.kind == AvailableSpace::Kind::Definite && b.IsSome()) {
        return AvailableSpace::Definite(F32Max(a.value, b.val));
    }
    return a;
}

inline AvailableSpace MaybeAdd(AvailableSpace a, Optf b) {
    if (a.kind == AvailableSpace::Kind::Definite && b.IsSome()) {
        return AvailableSpace::Definite(a.value + b.val);
    }
    return a;
}

inline AvailableSpace MaybeSub(AvailableSpace a, Optf b) {
    if (a.kind == AvailableSpace::Kind::Definite && b.IsSome()) {
        return AvailableSpace::Definite(a.value - b.val);
    }
    return a;
}

inline AvailableSpace MaybeClamp(AvailableSpace a, Optf lo, Optf hi) {
    if (a.kind != AvailableSpace::Kind::Definite) {
        return a;
    }
    float v = a.value;
    if (hi.IsSome()) {
        v = F32Min(v, hi.val);
    }
    if (lo.IsSome()) {
        v = F32Max(v, lo.val);
    }
    return AvailableSpace::Definite(v);
}

// ─── the same, component-wise over a size ────────────────────────────────

inline SizeOptF MaybeMin(SizeOptF a, SizeOptF b) {
    return {MaybeMin(a.width, b.width), MaybeMin(a.height, b.height)};
}

inline SizeOptF MaybeMax(SizeOptF a, SizeOptF b) {
    return {MaybeMax(a.width, b.width), MaybeMax(a.height, b.height)};
}

inline SizeOptF MaybeMax(SizeOptF a, SizeF b) {
    return {MaybeMax(a.width, b.width), MaybeMax(a.height, b.height)};
}

inline SizeOptF MaybeAdd(SizeOptF a, SizeF b) {
    return {MaybeAdd(a.width, b.width), MaybeAdd(a.height, b.height)};
}

inline SizeOptF MaybeSub(SizeOptF a, SizeF b) {
    return {MaybeSub(a.width, b.width), MaybeSub(a.height, b.height)};
}

inline SizeOptF MaybeSub(SizeOptF a, SizeOptF b) {
    return {MaybeSub(a.width, b.width), MaybeSub(a.height, b.height)};
}

inline SizeOptF MaybeClamp(SizeOptF a, SizeOptF lo, SizeOptF hi) {
    return {MaybeClamp(a.width, lo.width, hi.width),
            MaybeClamp(a.height, lo.height, hi.height)};
}

inline SizeF MaybeClamp(SizeF a, SizeOptF lo, SizeOptF hi) {
    return {MaybeClamp(a.width, lo.width, hi.width),
            MaybeClamp(a.height, lo.height, hi.height)};
}

inline SizeF MaybeMax(SizeF a, SizeOptF b) {
    return {MaybeMax(a.width, b.width), MaybeMax(a.height, b.height)};
}

inline SizeF MaybeMin(SizeF a, SizeOptF b) {
    return {MaybeMin(a.width, b.width), MaybeMin(a.height, b.height)};
}

inline SizeF MaybeAdd(SizeF a, SizeOptF b) {
    return {MaybeAdd(a.width, b.width), MaybeAdd(a.height, b.height)};
}

inline SizeF MaybeSub(SizeF a, SizeOptF b) {
    return {MaybeSub(a.width, b.width), MaybeSub(a.height, b.height)};
}

inline SizeAvail MaybeSub(SizeAvail a, SizeF b) {
    return {MaybeSub(a.width, b.width), MaybeSub(a.height, b.height)};
}

inline SizeAvail MaybeClamp(SizeAvail a, SizeOptF lo, SizeOptF hi) {
    return {MaybeClamp(a.width, lo.width, hi.width),
            MaybeClamp(a.height, lo.height, hi.height)};
}

inline SizeAvail MaybeSet(SizeAvail a, SizeOptF v) {
    return a.MaybeSet(v);
}

// Rust writes this as `Size::map(Some)` — a definite size seen as an
// everywhere-Some optional one.
inline SizeOptF AsOptional(SizeF s) {
    return {Optf(s.width), Optf(s.height)};
}

} // namespace taffy

#endif // GPUI_TAFFY_MATH_H_
