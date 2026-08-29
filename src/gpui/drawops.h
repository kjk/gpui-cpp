#ifndef GPUI_GPUI_DRAWOPS_H_
#define GPUI_GPUI_DRAWOPS_H_
#include "gpui/gpui.h"

// A drawing as a byte array: a stream of ops, each a `u16` opcode followed by
// its arguments. Floats are little-endian `f32`, colours a little-endian
// `u32` 0xRRGGBBAA. The stream ends at `kOpEnd` or at the end of the buffer.
//
//     OpLine <float> <float> <float> <float>
//
// Coordinates are in the drawing's own viewBox — 24x24 for a lucide icon —
// and `ExecuteDrawOps` maps that box onto the destination box it is given, so
// one encoding draws at every size. Everything an icon here is made of is in
// this format: `assets/icons` is compiled into one byte array by
// `cmd/svg-to-bytecode.ts` (`asset_icons.h`), and an `.svg` that is not in it
// is converted at load time by `SvgToDrawOps` and drawn the same way.

namespace gpui {

// The opcodes. Append only: a number here is in the generated
// `asset_icons.cpp`, so renumbering one silently redraws every icon.
enum DrawOp : uint8_t {
    kOpEnd = 0,
    // The coordinate system every following op is written in. Without one the
    // drawing is read as 0 0 24 24.
    kOpViewBox = 1, // x y w h
    // How wide a stroke is, in viewBox units, the way SVG's stroke-width is.
    kOpStrokeWidth = 2, // w
    // Paint what follows in a colour of its own instead of the caller's. A
    // two-tone logo names one; a lucide icon never does.
    kOpColor = 3, // rgba
    kOpColorReset = 4,

    // ─── shapes ───
    kOpLine = 5,        // x1 y1 x2 y2
    kOpRect = 6,        // x y w h r     (stroked, r is the corner radius)
    kOpFillRect = 7,    // x y w h r
    kOpEllipse = 8,     // cx cy rx ry   (stroked)
    kOpFillEllipse = 9, // cx cy rx ry
    // A stretch of the circle at (cx, cy), as a stroked polyline. Angles are
    // degrees, zero at three o'clock and growing clockwise, which is the way
    // the y axis runs.
    kOpArc = 10, // cx cy r a0 a1

    // ─── paths ───
    //
    // Move/Line/Cubic/Close build one; a Fill/Stroke op paints it and starts
    // the next one.
    kOpMoveTo = 11,  // x y
    kOpLineTo = 12,  // x y
    kOpCubicTo = 13, // x1 y1 x2 y2 x y
    kOpClosePath = 14,
    kOpFillPath = 15,
    kOpStrokePath = 16,
    // Both, in that order: a solid lucide variant (star-fill) is filled and
    // then stroked, so its outline is as heavy as the hollow one beside it.
    kOpFillStrokePath = 17,

    // ─── text ───
    //
    //     kOpText  <x> <y>  <size> <textLength>  <u32 flags>  <u16 len> <utf8>
    //
    // `x`/`y` is the anchor point SVG names, on the baseline; `size` is the
    // font size in viewBox units, so it scales with the box the way a stroke
    // width does. `textLength` is SVG's, or 0 for a run that named none.
    // Nothing under `assets/icons` has a <text>, so `asset_icons.cpp` never
    // holds one of these and `cmd/svg-to-bytecode.ts` has nothing to write.
    kOpText = 18,
};

// kOpText's flags word: which end of the run the anchor point is, and whether
// it is bold. `text-anchor` and `font-weight`, the two of that element's
// presentation attributes a run of glyphs here can answer for.
enum DrawOpTextFlag : uint8_t {
    kTextAnchorMask = 3,
    kTextAnchorStart = 0,
    kTextAnchorMiddle = 1,
    kTextAnchorEnd = 2,
    kTextBold = 4,
};

// Where a drawing goes and what colour it takes. `turns` rotates it clockwise
// about the middle of the box, 1 being a whole turn — GPUI's
// `Transformation::rotate(percentage(..))`, applied as the path is built
// rather than by the backend.
struct DrawOpsTarget {
    float x = 0;
    float y = 0;
    float w = 16;
    float h = 16;
    // The colour of every op that did not name one of its own.
    Rgba color = {};
    float turns = 0;
};

// Walks `data` and paints it. False if there is nothing there to paint.
bool ExecuteDrawOps(PaintCtx* ctx, const void* data, int dataLen,
                    const DrawOpsTarget& t);

// The viewBox a drawing was authored in, so a caller that knows one dimension
// can work out the other. False when the buffer is not a drawing.
bool DrawOpsViewBox(const void* data, int dataLen, Size* out);

// ─── writing one ──────────────────────────────────────────────────────────
//
// `cmd/svg-to-bytecode.ts` writes the same bytes from TypeScript. The two
// have to agree, so a change to what `SvgToDrawOps` emits belongs in both.

struct DrawOpsBuilder {
    Vec<uint8_t> data;

    void Op(DrawOp op);
    // Coordinates come in pairs — a point, a size, one control of a cubic —
    // so one append writes two of them. There is no single-float write: the
    // stroke width is the only odd argument in the format and StrokeWidth()
    // puts it in the same append as its opcode.
    void F2(float a, float b);
    void U32(uint32_t v);

    void ViewBox(float x, float y, float w, float h);
    void StrokeWidth(float w);
    void Color(Rgba c);
    void ColorReset();
    void Line(float x1, float y1, float x2, float y2);
    void MoveTo(float x, float y);
    void LineTo(float x, float y);
    void CubicTo(float x1, float y1, float x2, float y2, float x, float y);
    void ClosePath();
    // `s` is copied into the stream, so the caller's bytes need not outlive
    // the builder. A run longer than 65535 bytes is truncated to a character
    // boundary rather than refused; nothing draws a novel.
    void Text(float x, float y, float size, float textLength, uint32_t flags,
              Str s);
    void End();
};

} // namespace gpui
#endif // GPUI_GPUI_DRAWOPS_H_
