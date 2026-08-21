/* Geometric primitives useful for layout — taffy/src/geometry.rs
 *
 * Part of the C++ port of taffy 0.12.2 (see src/taffy/readme.md).
 *
 * Deviations from the Rust, both forced by this tree's rules:
 *   - Rust's `Point<T>`, `Size<T>`, `Rect<T>` and `Line<T>` are generic over
 *     the element type. Here each instantiation taffy actually uses is its own
 *     struct: the float ones (`SizeF`, `PointF`, `RectF`, `LineF`) live here,
 *     and the style-typed ones (`SizeDim`, `SizeAvail`, `RectLp`, …) live in
 *     style.h next to the types they hold. Each carries only the operations
 *     taffy performs on it, which is why they are not all alike.
 *   - `Option<f32>` is `Optf`. The optional enums taffy carries get one
 *     concrete struct each, also in style.h.
 */

#ifndef GPUI_TAFFY_GEOMETRY_H_
#define GPUI_TAFFY_GEOMETRY_H_

#include "base.h"

#include <cmath>
#include <cfloat>

namespace taffy {

using gpui::Arena;
using gpui::Str;
using gpui::Vec;

// ─── Option<f32> ─────────────────────────────────────────────────────────

// Rust's `Option<f32>`. POD, so it lives in a Vec and memcpys.
struct Optf {
    float val = 0.0f;
    bool has = false;

    constexpr Optf() = default;
    constexpr explicit Optf(float v) : val(v), has(true) {}

    constexpr bool IsSome() const { return has; }
    constexpr bool IsNone() const { return !has; }
    // Rust's `Option::unwrap`. A None here is a layout bug, not input.
    constexpr float Unwrap() const { return val; }
    constexpr float UnwrapOr(float alt) const { return has ? val : alt; }
    constexpr Optf Or(Optf alt) const { return has ? *this : alt; }
};

// Rust spells these `Some(x)` and `None`.
constexpr Optf Some(float v) {
    return Optf(v);
}

constexpr Optf None() {
    return Optf();
}

constexpr bool operator==(Optf a, Optf b) {
    return a.has == b.has && (!a.has || a.val == b.val);
}

constexpr bool operator!=(Optf a, Optf b) {
    return !(a == b);
}

// ─── f32 helpers — taffy/src/util/sys.rs ─────────────────────────────────
//
// Rust's `f32::max` / `f32::min` return the non-NaN operand when one side is
// NaN; `std::max` does not, and taffy leans on that.

inline float F32Max(float a, float b) {
    if (std::isnan(a)) {
        return b;
    }
    if (std::isnan(b)) {
        return a;
    }
    return a > b ? a : b;
}

inline float F32Min(float a, float b) {
    if (std::isnan(a)) {
        return b;
    }
    if (std::isnan(b)) {
        return a;
    }
    return a < b ? a : b;
}

// Rust spells this `(value + 0.5).floor()` — round half up, not half away
// from zero.
inline float F32Round(float v) {
    return floorf(v + 0.5f);
}

// ─── axes ────────────────────────────────────────────────────────────────

// The simple absolute horizontal and vertical axis.
enum class AbsoluteAxis : uint8_t {
    Horizontal,
    Vertical
};

constexpr AbsoluteAxis OtherAxis(AbsoluteAxis axis) {
    return axis == AbsoluteAxis::Horizontal ? AbsoluteAxis::Vertical
                                            : AbsoluteAxis::Horizontal;
}

// The CSS abstract axis.
// https://www.w3.org/TR/css-writing-modes-3/#abstract-axes
enum class AbstractAxis : uint8_t {
    Inline,
    Block
};

constexpr AbstractAxis Other(AbstractAxis axis) {
    return axis == AbstractAxis::Inline ? AbstractAxis::Block
                                        : AbstractAxis::Inline;
}

// Convert an AbstractAxis into an AbsoluteAxis, naively assuming the Inline
// axis is Horizontal. Always true until taffy implements writing_mode.
constexpr AbsoluteAxis AsAbsNaive(AbstractAxis axis) {
    return axis == AbstractAxis::Inline ? AbsoluteAxis::Horizontal
                                        : AbsoluteAxis::Vertical;
}

// FlexDirection belongs to the style layer, but every container below keys
// its main/cross helpers off it, so it is declared here.
enum class FlexDirection : uint8_t {
    Row,
    Column,
    RowReverse,
    ColumnReverse
};

constexpr bool IsRow(FlexDirection d) {
    return d == FlexDirection::Row || d == FlexDirection::RowReverse;
}

constexpr bool IsColumn(FlexDirection d) {
    return d == FlexDirection::Column || d == FlexDirection::ColumnReverse;
}

constexpr bool IsReverse(FlexDirection d) {
    return d == FlexDirection::RowReverse || d == FlexDirection::ColumnReverse;
}

constexpr AbsoluteAxis MainAxis(FlexDirection d) {
    return IsRow(d) ? AbsoluteAxis::Horizontal : AbsoluteAxis::Vertical;
}

constexpr AbsoluteAxis CrossAxis(FlexDirection d) {
    return IsRow(d) ? AbsoluteAxis::Vertical : AbsoluteAxis::Horizontal;
}

// ─── SizeF ───────────────────────────────────────────────────────────────

// Rust's `Size<f32>`.
struct SizeF {
    float width = 0.0f;
    float height = 0.0f;

    static constexpr SizeF Zero() { return {0.0f, 0.0f}; }

    constexpr float GetAbs(AbsoluteAxis a) const {
        return a == AbsoluteAxis::Horizontal ? width : height;
    }
    constexpr float Get(AbstractAxis a) const {
        return a == AbstractAxis::Inline ? width : height;
    }
    constexpr void Set(AbstractAxis a, float v) {
        if (a == AbstractAxis::Inline) {
            width = v;
        } else {
            height = v;
        }
    }
    constexpr float Main(FlexDirection d) const {
        return IsRow(d) ? width : height;
    }
    constexpr float Cross(FlexDirection d) const {
        return IsRow(d) ? height : width;
    }
    constexpr void SetMain(FlexDirection d, float v) {
        if (IsRow(d)) {
            width = v;
        } else {
            height = v;
        }
    }
    constexpr void SetCross(FlexDirection d, float v) {
        if (IsRow(d)) {
            height = v;
        } else {
            width = v;
        }
    }
    constexpr SizeF WithMain(FlexDirection d, float v) const {
        SizeF out = *this;
        out.SetMain(d, v);
        return out;
    }
    constexpr SizeF WithCross(FlexDirection d, float v) const {
        SizeF out = *this;
        out.SetCross(d, v);
        return out;
    }
    // Both components clamped against another size.
    SizeF Max(SizeF rhs) const {
        return {F32Max(width, rhs.width), F32Max(height, rhs.height)};
    }
    SizeF Min(SizeF rhs) const {
        return {F32Min(width, rhs.width), F32Min(height, rhs.height)};
    }
    constexpr bool HasNonZeroArea() const {
        return width > 0.0f && height > 0.0f;
    }
};

constexpr bool operator==(SizeF a, SizeF b) {
    return a.width == b.width && a.height == b.height;
}

constexpr bool operator!=(SizeF a, SizeF b) {
    return !(a == b);
}

constexpr SizeF operator+(SizeF a, SizeF b) {
    return {a.width + b.width, a.height + b.height};
}

constexpr SizeF operator-(SizeF a, SizeF b) {
    return {a.width - b.width, a.height - b.height};
}

// ─── SizeOptF ────────────────────────────────────────────────────────────

// Rust's `Size<Option<f32>>`.
struct SizeOptF {
    Optf width;
    Optf height;

    static constexpr SizeOptF None() { return {}; }
    static constexpr SizeOptF New(float w, float h) {
        return {Optf(w), Optf(h)};
    }
    // Rust's `Size::<Option<f32>>::from_cross`.
    static constexpr SizeOptF FromCross(FlexDirection d, Optf v) {
        SizeOptF out;
        if (IsRow(d)) {
            out.height = v;
        } else {
            out.width = v;
        }
        return out;
    }

    constexpr Optf GetAbs(AbsoluteAxis a) const {
        return a == AbsoluteAxis::Horizontal ? width : height;
    }
    constexpr Optf Get(AbstractAxis a) const {
        return a == AbstractAxis::Inline ? width : height;
    }
    constexpr void Set(AbstractAxis a, Optf v) {
        if (a == AbstractAxis::Inline) {
            width = v;
        } else {
            height = v;
        }
    }
    constexpr Optf Main(FlexDirection d) const {
        return IsRow(d) ? width : height;
    }
    constexpr Optf Cross(FlexDirection d) const {
        return IsRow(d) ? height : width;
    }
    constexpr void SetMain(FlexDirection d, Optf v) {
        if (IsRow(d)) {
            width = v;
        } else {
            height = v;
        }
    }
    constexpr void SetCross(FlexDirection d, Optf v) {
        if (IsRow(d)) {
            height = v;
        } else {
            width = v;
        }
    }
    constexpr SizeF UnwrapOr(SizeF alt) const {
        return {width.UnwrapOr(alt.width), height.UnwrapOr(alt.height)};
    }
    constexpr SizeOptF Or(SizeOptF alt) const {
        return {width.Or(alt.width), height.Or(alt.height)};
    }
    constexpr bool BothAxisDefined() const {
        return width.IsSome() && height.IsSome();
    }
    // If one axis is Some and the other None, fill the None one in from the
    // ratio. Anything else is returned unchanged.
    constexpr SizeOptF MaybeApplyAspectRatio(Optf aspectRatio) const {
        if (!aspectRatio.IsSome()) {
            return *this;
        }
        float ratio = aspectRatio.val;
        if (width.IsSome() && !height.IsSome()) {
            return {width, Optf(width.val / ratio)};
        }
        if (!width.IsSome() && height.IsSome()) {
            return {Optf(height.val * ratio), height};
        }
        return *this;
    }
};

constexpr bool operator==(SizeOptF a, SizeOptF b) {
    return a.width == b.width && a.height == b.height;
}

constexpr bool operator!=(SizeOptF a, SizeOptF b) {
    return !(a == b);
}

// ─── PointF / PointOptF ──────────────────────────────────────────────────

// A 2-dimensional coordinate. With a rect, this is the top-left corner.
struct PointF {
    float x = 0.0f;
    float y = 0.0f;

    static constexpr PointF Zero() { return {0.0f, 0.0f}; }

    constexpr float Get(AbstractAxis a) const {
        return a == AbstractAxis::Inline ? x : y;
    }
    constexpr void Set(AbstractAxis a, float v) {
        if (a == AbstractAxis::Inline) {
            x = v;
        } else {
            y = v;
        }
    }
    constexpr float Main(FlexDirection d) const { return IsRow(d) ? x : y; }
    constexpr float Cross(FlexDirection d) const { return IsRow(d) ? y : x; }
    constexpr PointF Transpose() const { return {y, x}; }
    constexpr SizeF IntoSize() const { return {x, y}; }
};

constexpr bool operator==(PointF a, PointF b) {
    return a.x == b.x && a.y == b.y;
}

constexpr bool operator!=(PointF a, PointF b) {
    return !(a == b);
}

constexpr PointF operator+(PointF a, PointF b) {
    return {a.x + b.x, a.y + b.y};
}

struct PointOptF {
    Optf x;
    Optf y;

    static constexpr PointOptF None() { return {}; }
};

// ─── LineF / LineBool ────────────────────────────────────────────────────

// An abstract "line": anything with a start and an end.
struct LineF {
    float start = 0.0f;
    float end = 0.0f;

    constexpr float Sum() const { return start + end; }
};

struct LineBool {
    bool start = false;
    bool end = false;

    static constexpr LineBool True() { return {true, true}; }
    static constexpr LineBool False() { return {false, false}; }
};

// ─── RectF / RectOptF ────────────────────────────────────────────────────

// An axis-aligned rectangle, or a set of four edge values (padding, border,
// margin). The axis sums below are the *edge totals*, not a width or height.
struct RectF {
    float left = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
    float bottom = 0.0f;

    static constexpr RectF Zero() { return {0.0f, 0.0f, 0.0f, 0.0f}; }
    static constexpr RectF New(float l, float r, float t, float b) {
        return {l, r, t, b};
    }

    constexpr float HorizontalAxisSum() const { return left + right; }
    constexpr float VerticalAxisSum() const { return top + bottom; }
    constexpr SizeF SumAxes() const { return {left + right, top + bottom}; }
    constexpr float GridAxisSum(AbsoluteAxis a) const {
        return a == AbsoluteAxis::Horizontal ? left + right : top + bottom;
    }
    constexpr float MainAxisSum(FlexDirection d) const {
        return IsRow(d) ? left + right : top + bottom;
    }
    constexpr float CrossAxisSum(FlexDirection d) const {
        return IsRow(d) ? top + bottom : left + right;
    }
    constexpr float MainStart(FlexDirection d) const {
        return IsRow(d) ? left : top;
    }
    constexpr float MainEnd(FlexDirection d) const {
        return IsRow(d) ? right : bottom;
    }
    constexpr float CrossStart(FlexDirection d) const {
        return IsRow(d) ? top : left;
    }
    constexpr float CrossEnd(FlexDirection d) const {
        return IsRow(d) ? bottom : right;
    }
    constexpr LineF HorizontalComponents() const { return {left, right}; }
    constexpr LineF VerticalComponents() const { return {top, bottom}; }
};

constexpr bool operator==(RectF a, RectF b) {
    return a.left == b.left && a.right == b.right && a.top == b.top &&
           a.bottom == b.bottom;
}

constexpr bool operator!=(RectF a, RectF b) {
    return !(a == b);
}

constexpr RectF operator+(RectF a, RectF b) {
    return {a.left + b.left, a.right + b.right, a.top + b.top,
            a.bottom + b.bottom};
}

struct RectOptF {
    Optf left;
    Optf right;
    Optf top;
    Optf bottom;

    constexpr Optf MainStart(FlexDirection d) const {
        return IsRow(d) ? left : top;
    }
    constexpr Optf MainEnd(FlexDirection d) const {
        return IsRow(d) ? right : bottom;
    }
    constexpr Optf CrossStart(FlexDirection d) const {
        return IsRow(d) ? top : left;
    }
    constexpr Optf CrossEnd(FlexDirection d) const {
        return IsRow(d) ? bottom : right;
    }
};

// ─── slices ──────────────────────────────────────────────────────────────
//
// The one template left. Rust's grid styles hold `Vec<T>`s; a `Vec<T>` here
// owns heap and runs a destructor, which is the wrong shape for a value type
// that is copied as bytes (hard rule 4). A `Slice<T>` is a view of memory the
// caller's arena owns, so a `Style` owns nothing.

template <typename T>
struct Slice {
    T* els = nullptr;
    int len = 0;

    T& operator[](int i) const { return els[i]; }
    bool IsEmpty() const { return len == 0; }
    T* begin() const { return els; }
    T* end() const { return els + len; }
};

// `n` uninitialised (zeroed) elements from `a`.
template <typename T>
Slice<T> SliceNew(Arena* a, int n) {
    if (n <= 0) {
        return {};
    }
    return {(T*)a->Alloc((int)sizeof(T) * n), n};
}

// A copy of `n` elements from `src` into `a`.
template <typename T>
Slice<T> SliceDup(Arena* a, const T* src, int n) {
    Slice<T> out = SliceNew<T>(a, n);
    if (out.els && src) {
        memcpy((void*)out.els, (const void*)src, sizeof(T) * (size_t)n);
    }
    return out;
}

// One element, which is what most `grid-auto-*` lists are.
template <typename T>
Slice<T> SliceOne(Arena* a, const T& v) {
    Slice<T> out = SliceNew<T>(a, 1);
    if (out.els) {
        out.els[0] = v;
    }
    return out;
}

} // namespace taffy

#endif // GPUI_TAFFY_GEOMETRY_H_
