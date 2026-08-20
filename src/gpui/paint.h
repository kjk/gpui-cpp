/* The 2D drawing surface, one signature per operation the element tree needs.
   Direct2D + DirectWrite behind Paint_win.cpp, cairo + Pango behind
   Paint_linux.cpp. Everything above this line is portable.

   Coordinates are DIPs with y growing down, matching the element tree. */

#include "gpui/gpui.h"

namespace gpui {

// Text weight byte: the weight in the low bits plus family / decoration
// flags, so the shaped-text cache keys mono and proportional runs apart on
// its own. Both backends decode the same byte.
enum {
    kFontWeightMask = 3,
    kFontWeightNormal = 0,
    kFontWeightSemibold = 1,
    kFontWeightBold = 2,
    kFontWeightMedium = 3,
    kFontMono = 4,
    kFontUnderline = 8,
    kFontItalic = 16
};

// GPUI lays every line of text into a box phi times the font size — the
// default TextStyle::line_height (gpui::phi(), geometry.rs) — and centers the
// glyphs in it. Both text engines are tighter than that on their own, so
// without it every text block, and every row that shrink-wraps one, comes out
// shorter than the original.
const float kLineHeight = 1.618034f;

// ─── backend lifecycle ────────────────────────────────────────────────────

// Create the factories and the shared font set. Null on failure.
PaintApp* PaintAppNew();
void PaintAppFree(PaintApp* pa);

// Bind `native` — the HDC on Windows, the cairo surface on Linux — as this
// frame's target and open a drawing batch. False means skip the frame.
bool PaintTargetBegin(PaintCtx* ctx, void* native, int pxW, int pxH);
// An offscreen square of pixels rather than a window: transparent to start
// with, and read back as premultiplied BGRA, top-down, by
// PaintTargetEndOffscreen. What a menu icon is rasterized through — the OS
// wants a bitmap of one, not an element that draws it.
bool PaintTargetBeginOffscreen(PaintCtx* ctx, int pxW, int pxH);
// `outBgra` takes pxW * pxH * 4 bytes.
bool PaintTargetEndOffscreen(PaintCtx* ctx, uint8_t* outBgra);
// Close the batch. Returns false if the device was lost and the target was
// dropped; the next frame recreates it.
bool PaintTargetEnd(PaintCtx* ctx);
// Drop the cached target: a DPI change, a resize, or a lost device.
void PaintTargetFree(PaintCtx* ctx);

// ─── canvas ───────────────────────────────────────────────────────────────

void CanvasClear(PaintCtx* ctx, Rgba c);
void CanvasFillRect(PaintCtx* ctx, float x, float y, float w, float h, Rgba c);
void CanvasFillRound(PaintCtx* ctx, float x, float y, float w, float h, float r,
                     Rgba c);
// `dash` is a two-element {on, off} pattern in stroke widths, or null for a
// solid line.
void CanvasStrokeRound(PaintCtx* ctx, float x, float y, float w, float h,
                       float r, float stroke, Rgba c,
                       const float* dash = nullptr);
void CanvasLine(PaintCtx* ctx, float x1, float y1, float x2, float y2,
                float stroke, Rgba c, const float* dash = nullptr);
// stroke <= 0 fills the ellipse instead of stroking it.
void CanvasEllipse(PaintCtx* ctx, float cx, float cy, float rx, float ry,
                   float stroke, Rgba c);
void CanvasPushClip(PaintCtx* ctx, float x, float y, float w, float h);
void CanvasPopClip(PaintCtx* ctx);

// ─── paths ────────────────────────────────────────────────────────────────
//
// Build, draw, free. There is no retained geometry: an icon or a chart area
// is rebuilt every frame, which is what both backends want anyway.

struct Path;

// `winding` picks the nonzero fill rule; false is even-odd.
Path* PathNew(PaintCtx* ctx, bool winding);
void PathFree(Path* p);
void PathMoveTo(Path* p, float x, float y);
void PathLineTo(Path* p, float x, float y);
void PathCubicTo(Path* p, float x1, float y1, float x2, float y2, float x,
                 float y);
// An arc of the circle at (cx, cy), from a0 to a1 radians measured clockwise
// from +x in this y-down space. Starts a figure if none is open.
void PathArcTo(Path* p, float cx, float cy, float r, float a0, float a1,
               bool clockwise);
void PathClose(Path* p);

void PathFill(PaintCtx* ctx, Path* p, Rgba c);
// A vertical linear gradient from `top` at y0 to `bot` at y1.
// A linear gradient between two points, which is what a sankey ribbon wants:
// its two ends are side by side, not one above the other.
void PathFillGradient(PaintCtx* ctx, Path* p, float x0, float y0, float x1,
                      float y1, Rgba from, Rgba to);
void PathFillGradientV(PaintCtx* ctx, Path* p, float y0, float y1, Rgba top,
                       Rgba bot);
void PathStroke(PaintCtx* ctx, Path* p, float stroke, Rgba c,
                bool roundCaps = false);

// ─── shaped text ──────────────────────────────────────────────────────────
//
// A TextLayout is one shaped run, refcounted so the measurement cache in
// Gpui.cpp can hold on to it across frames.

struct TextLayout;

// Shape `s` and report its size. maxW <= 0 is unconstrained. Null if the text
// is empty or shaping failed.
TextLayout* TextLayoutNew(PaintCtx* ctx, Str s, float fontSize, float maxW,
                          bool wrap, uint8_t weight, float lineH,
                          Size* outSize);
void TextLayoutAddRef(TextLayout* tl);
void TextLayoutRelease(TextLayout* tl);
// `clip` confines the glyphs to the layout box — text_overflow: clip. That
// box is the wrap width the layout was built with, which for a truncating
// element is its own width.
void TextLayoutDraw(PaintCtx* ctx, TextLayout* tl, float x, float y, Rgba c,
                    bool clip);
// The UTF-8 offset into `s` nearest the layout-relative point.
int TextLayoutHitPoint(TextLayout* tl, Str s, float relX, float relY);
// The rectangles covering UTF-8 range [u8a, u8b), one per line. Returns how
// many were written.
int TextLayoutRangeRects(TextLayout* tl, Str s, int u8a, int u8b, Bounds* out,
                         int max);

} // namespace gpui
