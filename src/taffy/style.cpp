/* Out-of-line half of the style layer — taffy/src/style/*.rs, plus the
 * resolution helpers from taffy/src/util/resolve.rs that turn a
 * context-dependent dimension into a concrete one.
 */

#include "taffy/style.h"

namespace taffy {

// ─── CompactLength ───────────────────────────────────────────────────────

float CompactLength::Value() const {
    uint32_t word = (uint32_t)(bits >> 32);
    float out = 0.0f;
    memcpy(&out, &word, sizeof(out));
    return out;
}

CompactLength CompactLength::FromVal(float v, uint64_t tag) {
    uint32_t word = 0;
    memcpy(&word, &v, sizeof(word));
    return CompactLength{((uint64_t)word << 32) | tag};
}

// ─── resolve — taffy/src/util/resolve.rs ─────────────────────────────────
//
// Rust's `MaybeResolve` returns None when it cannot resolve; `ResolveOrZero`
// falls back to zero. Both are trait impls over three newtypes there, which
// is one function per newtype here.

static Optf MaybeResolveRaw(CompactLength raw, Optf context,
                            CalcResolver calc) {
    switch (raw.Tag()) {
        case CompactLength::kAutoTag:
            return None();
        case CompactLength::kLengthTag:
            return Some(raw.Value());
        case CompactLength::kPercentTag:
            return IsSome(context) ? Some(context * raw.Value()) : None();
        default:
            break;
    }
    if (raw.IsCalc() && IsSome(context)) {
        return Some(calc.Resolve(raw.CalcValue(), context));
    }
    return None();
}

static float ResolveOrZeroRaw(CompactLength raw, Optf context,
                              CalcResolver calc) {
    return UnwrapOr(MaybeResolveRaw(raw, context, calc), 0.0f);
}

Optf LengthPercentageAuto::ResolveToOption(float context,
                                           CalcResolver calc) const {
    return MaybeResolveRaw(raw, Some(context), calc);
}

Optf LengthPercentageAuto::MaybeResolve(Optf context, CalcResolver calc) const {
    return MaybeResolveRaw(raw, context, calc);
}

Optf Dimension::MaybeResolve(Optf context, CalcResolver calc) const {
    return MaybeResolveRaw(raw, context, calc);
}

SizeOptF SizeDim::MaybeResolve(SizeOptF context, CalcResolver calc) const {
    return {MaybeResolveRaw(width.raw, context.w, calc),
            MaybeResolveRaw(height.raw, context.h, calc)};
}

SizeF SizeDim::ResolveOrZero(SizeOptF context, CalcResolver calc) const {
    return {ResolveOrZeroRaw(width.raw, context.w, calc),
            ResolveOrZeroRaw(height.raw, context.h, calc)};
}

SizeF SizeLp::ResolveOrZero(SizeOptF context, CalcResolver calc) const {
    return {ResolveOrZeroRaw(width.raw, context.w, calc),
            ResolveOrZeroRaw(height.raw, context.h, calc)};
}

SizeF SizeLp::ResolveOrZero(Optf context, CalcResolver calc) const {
    return {ResolveOrZeroRaw(width.raw, context, calc),
            ResolveOrZeroRaw(height.raw, context, calc)};
}

// A Rect resolves its left/right against the width and its top/bottom against
// the height, which is why the Size and the Optf overloads differ.
RectF RectLp::ResolveOrZero(SizeOptF context, CalcResolver calc) const {
    return {ResolveOrZeroRaw(left.raw, context.w, calc),
            ResolveOrZeroRaw(right.raw, context.w, calc),
            ResolveOrZeroRaw(top.raw, context.h, calc),
            ResolveOrZeroRaw(bottom.raw, context.h, calc)};
}

RectF RectLp::ResolveOrZero(Optf context, CalcResolver calc) const {
    return {ResolveOrZeroRaw(left.raw, context, calc),
            ResolveOrZeroRaw(right.raw, context, calc),
            ResolveOrZeroRaw(top.raw, context, calc),
            ResolveOrZeroRaw(bottom.raw, context, calc)};
}

RectF RectLpa::ResolveOrZero(SizeOptF context, CalcResolver calc) const {
    return {ResolveOrZeroRaw(left.raw, context.w, calc),
            ResolveOrZeroRaw(right.raw, context.w, calc),
            ResolveOrZeroRaw(top.raw, context.h, calc),
            ResolveOrZeroRaw(bottom.raw, context.h, calc)};
}

RectF RectLpa::ResolveOrZero(Optf context, CalcResolver calc) const {
    return {ResolveOrZeroRaw(left.raw, context, calc),
            ResolveOrZeroRaw(right.raw, context, calc),
            ResolveOrZeroRaw(top.raw, context, calc),
            ResolveOrZeroRaw(bottom.raw, context, calc)};
}

RectOptF RectLpa::MaybeResolve(Optf context, CalcResolver calc) const {
    return {MaybeResolveRaw(left.raw, context, calc),
            MaybeResolveRaw(right.raw, context, calc),
            MaybeResolveRaw(top.raw, context, calc),
            MaybeResolveRaw(bottom.raw, context, calc)};
}

RectOptF RectLpa::MaybeResolveZip(SizeOptF context, CalcResolver calc) const {
    return {MaybeResolveRaw(left.raw, context.w, calc),
            MaybeResolveRaw(right.raw, context.w, calc),
            MaybeResolveRaw(top.raw, context.h, calc),
            MaybeResolveRaw(bottom.raw, context.h, calc)};
}

// ─── grid coordinates ────────────────────────────────────────────────────

OriginZeroLine IntoOriginZeroLine(GridLine line, uint16_t explicitTrackCount) {
    int16_t explicitLineCount = (int16_t)(explicitTrackCount + 1);
    if (line.v > 0) {
        return OriginZeroLine{(int16_t)(line.v - 1)};
    }
    if (line.v < 0) {
        return OriginZeroLine{(int16_t)(line.v + explicitLineCount)};
    }
    // Grid line zero is invalid; every caller filters it out first.
    return OriginZeroLine{0};
}

// ─── grid placement ──────────────────────────────────────────────────────

static PlainPlacement IntoOriginZeroIgnoringNamed(const GridPlacement& p,
                                                  uint16_t explicitTrackCount) {
    switch (p.kind) {
        case GridPlacementKind::Span:
            return PlainPlacement::Spanning(p.span);
        case GridPlacementKind::Line:
            // Grid line zero is an invalid index, so it is treated as auto.
            if (p.line == 0) {
                return PlainPlacement::Auto();
            }
            return PlainPlacement::AtLine(
                IntoOriginZeroLine(GridLine{p.line}, explicitTrackCount).v);
        default:
            // Auto, NamedLine and NamedSpan all fall back to auto here; the
            // named ones are resolved by the caller before this point.
            return PlainPlacement::Auto();
    }
}

bool LinePlacement::IsDefinite() const {
    if (start.kind == GridPlacementKind::Line && start.line != 0) {
        return true;
    }
    if (end.kind == GridPlacementKind::Line && end.line != 0) {
        return true;
    }
    return start.kind == GridPlacementKind::NamedLine ||
           end.kind == GridPlacementKind::NamedLine;
}

LinePlain LinePlacement::IntoOriginZeroIgnoringNamed(
    uint16_t explicitTrackCount) const {
    return {taffy::IntoOriginZeroIgnoringNamed(start, explicitTrackCount),
            taffy::IntoOriginZeroIgnoringNamed(end, explicitTrackCount)};
}

bool LinePlain::IsDefinite() const {
    return start.IsLine() || end.IsLine();
}

bool LinePlain::IsDefiniteGridLine() const {
    return (start.IsLine() && start.line != 0) ||
           (end.IsLine() && end.line != 0);
}

static PlainPlacement PlainIntoOriginZero(PlainPlacement p,
                                          uint16_t explicitTrackCount) {
    if (p.IsSpan()) {
        return p;
    }
    if (p.IsLine()) {
        if (p.line == 0) {
            return PlainPlacement::Auto();
        }
        return PlainPlacement::AtLine(
            IntoOriginZeroLine(GridLine{p.line}, explicitTrackCount).v);
    }
    return PlainPlacement::Auto();
}

LinePlain LinePlain::IntoOriginZero(uint16_t explicitTrackCount) const {
    return {PlainIntoOriginZero(start, explicitTrackCount),
            PlainIntoOriginZero(end, explicitTrackCount)};
}

uint16_t LinePlain::IndefiniteSpan() const {
    if (start.IsSpan()) {
        return start.span;
    }
    if (end.IsSpan()) {
        return end.span;
    }
    // (Line, Auto), (Auto, Line) and (Auto, Auto) all span one track. Rust
    // panics on (Line, Line); the callers here only reach this with an
    // indefinite placement, and one track is the harmless answer.
    return 1;
}

LineOzl LinePlain::ResolveDefiniteGridLines() const {
    OriginZeroLine s = start.Ozl();
    OriginZeroLine e = end.Ozl();
    if (start.IsLine() && end.IsLine()) {
        if (s == e) {
            return {s, s + (uint16_t)1};
        }
        return s < e ? LineOzl{s, e} : LineOzl{e, s};
    }
    if (start.IsLine() && end.IsSpan()) {
        return {s, s + end.span};
    }
    if (start.IsLine()) {
        return {s, s + (uint16_t)1};
    }
    if (start.IsSpan() && end.IsLine()) {
        return {e - start.span, e};
    }
    // (Auto, Line)
    return {e - (uint16_t)1, e};
}

LineOptOzl LinePlain::ResolveAbsolutelyPositionedGridTracks() const {
    OriginZeroLine s = start.Ozl();
    OriginZeroLine e = end.Ozl();
    if (start.IsLine() && end.IsLine()) {
        if (s == e) {
            return {OptOriginZeroLine(s), OptOriginZeroLine(s + (uint16_t)1)};
        }
        if (s < e) {
            return {OptOriginZeroLine(s), OptOriginZeroLine(e)};
        }
        return {OptOriginZeroLine(e), OptOriginZeroLine(s)};
    }
    if (start.IsLine() && end.IsSpan()) {
        return {OptOriginZeroLine(s), OptOriginZeroLine(s + end.span)};
    }
    if (start.IsLine()) {
        return {OptOriginZeroLine(s), OptOriginZeroLine()};
    }
    if (start.IsSpan() && end.IsLine()) {
        return {OptOriginZeroLine(e - start.span), OptOriginZeroLine(e)};
    }
    if (end.IsLine()) {
        return {OptOriginZeroLine(), OptOriginZeroLine(e)};
    }
    return {};
}

LineOzl LinePlain::ResolveIndefiniteGridTracks(OriginZeroLine s) const {
    if (start.IsSpan()) {
        return {s, s + start.span};
    }
    if (end.IsSpan()) {
        return {s, s + end.span};
    }
    return {s, s + (uint16_t)1};
}

// ─── track sizing functions ──────────────────────────────────────────────

bool MaxTrackSizingFunction::HasDefiniteValue(Optf parentSize) const {
    switch (raw.Tag()) {
        case CompactLength::kLengthTag:
            return true;
        case CompactLength::kPercentTag:
            return IsSome(parentSize);
        default:
            return raw.IsCalc() && IsSome(parentSize);
    }
}

Optf MaxTrackSizingFunction::DefiniteValue(Optf parentSize,
                                           CalcResolver calc) const {
    switch (raw.Tag()) {
        case CompactLength::kLengthTag:
            return Some(raw.Value());
        case CompactLength::kPercentTag:
            return IsSome(parentSize) ? Some(raw.Value() * parentSize) : None();
        default:
            break;
    }
    if (raw.IsCalc() && IsSome(parentSize)) {
        return Some(calc.Resolve(raw.CalcValue(), parentSize));
    }
    return None();
}

Optf MaxTrackSizingFunction::DefiniteLimit(Optf parentSize,
                                           CalcResolver calc) const {
    switch (raw.Tag()) {
        case CompactLength::kFitContentPxTag:
            return Some(raw.Value());
        case CompactLength::kFitContentPercentTag:
            return IsSome(parentSize) ? Some(raw.Value() * parentSize) : None();
        default:
            return DefiniteValue(parentSize, calc);
    }
}

Optf MinTrackSizingFunction::DefiniteValue(Optf parentSize,
                                           CalcResolver calc) const {
    switch (raw.Tag()) {
        case CompactLength::kLengthTag:
            return Some(raw.Value());
        case CompactLength::kPercentTag:
            return IsSome(parentSize) ? Some(raw.Value() * parentSize) : None();
        default:
            break;
    }
    if (raw.IsCalc() && IsSome(parentSize)) {
        return Some(calc.Resolve(raw.CalcValue(), parentSize));
    }
    return None();
}

} // namespace taffy
