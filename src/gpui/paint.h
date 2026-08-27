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
    kFontWeightMask = 15,
    kFontWeightNormal = 0,
    kFontWeightThin = 1,
    kFontWeightExtraLight = 2,
    kFontWeightLight = 3,
    kFontWeightExplicitNormal = 4,
    kFontWeightMedium = 5,
    kFontWeightSemibold = 6,
    kFontWeightBold = 7,
    kFontWeightExtraBold = 8,
    kFontWeightBlack = 9,
    kFontMono = 16,
    kFontUnderline = 32,
    kFontItalic = 64,
    // text_decoration_line_through(): what a markdown `~~del~~` run and an
    // HTML <s> / <del> paint with. DirectWrite and Pango draw it themselves;
    // Core Text has no strikethrough attribute, so paint_mac draws the rule.
    kFontStrike = 128
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

// Bind `native` — the HWND on Windows, the cairo surface on Linux — as this
// frame's target and open a drawing batch. False means skip the frame.
//
// Windows takes the window, not a device context, because the frame goes to
// the window's own swap chain: D2D has no GPU path to an HDC, and the DC
// render target this used to be spent most of the frame copying its surface
// back through the GDI interop.
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

// The colour to actually paint: what the caller asked for, faded by the
// opacity in force. GPUI multiplies every primitive's colour by
// `element_opacity()` the same way, at the same moment — as the primitive is
// handed to the backend, not when the style was built.
inline Rgba PaintFade(const PaintCtx* ctx, Rgba c) {
    if (!ctx || ctx->opacity >= 1.f) {
        return c;
    }
    float a = (float)c.a * (ctx->opacity < 0 ? 0 : ctx->opacity);
    c.a = (uint8_t)(a <= 0 ? 0 : (a >= 255 ? 255 : a + 0.5f));
    return c;
}

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
// Say that `p` is about to be drawn more than once, so a backend that can
// pay a tessellation forward does it now: D2D builds a geometry realization,
// which is the one thing that makes a path cheap to fill twice. A backend
// with nothing to cache leaves this empty, and nothing has to call it — it is
// what src/gpui/scene.cpp calls when it puts a path in its cache.
void PathRealize(PaintCtx* ctx, Path* p);

// ─── images ───────────────────────────────────────────────────────────────
//
// A decoded bitmap. GPUI hands an `img(..)` element's source to its asset
// system, which decodes with the `image` crate; there is no such crate here
// and no room for one, so the decode is the platform's own: WIC on Windows,
// NSBitmapImageRep on macOS, and cairo's PNG loader on Linux — which is why
// Linux reads PNG and nothing else. gpui/image.h caches what comes back and
// is what the element tree talks to.

struct Image;

// Decode `bytes`. Null when the format is not one this platform reads, which
// the caller shows as the image's alt text.
Image* ImageDecode(PaintApp* pa, const uint8_t* bytes, int len);
void ImageFree(Image* img);
// The image's own size in pixels.
Size ImageSizePx(const Image* img);
// Draw it scaled into `b`. The caller has already picked the box, so this is
// a straight stretch — object_fit is decided above.
// `radius` rounds the corners the picture is drawn into, which is what an
// avatar is: `AvatarImage::new(src).size_full().rounded_full()`. Zero draws
// the plain rectangle. It is a parameter rather than a clip because a clip
// here is axis-aligned only, and the four backends all have a cheap way to
// fill a rounded rect with a picture.
void ImageDraw(PaintCtx* ctx, Image* img, Bounds b, float radius = 0);

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
// What TextLayoutNew reported as `outSize`, asked for again: the size the
// shaped run occupies, which is what a caller holding only the layout needs
// to know what area drawing it covers.
Size TextLayoutSize(TextLayout* tl);
void TextLayoutAddRef(TextLayout* tl);
void TextLayoutRelease(TextLayout* tl);
// `clip` cuts the run at `clipW` — GPUI's `truncate()`, which is
// `text_overflow: Ellipsis`, so what the backend draws is the run trimmed with
// an ellipsis rather than cut through a glyph. A non-wrapping run is shaped
// unconstrained (see TextMeasLayout), so the width has to come with the draw:
// the layout does not know the box it is going into. `clipW` of 0 leaves it
// to the caller's own clip.
void TextLayoutDraw(PaintCtx* ctx, TextLayout* tl, float x, float y, Rgba c,
                    bool clip, float clipW = 0);
// The UTF-8 offset into `s` nearest the layout-relative point.
int TextLayoutHitPoint(TextLayout* tl, Str s, float relX, float relY);
// The rectangles covering UTF-8 range [u8a, u8b), one per line. Returns how
// many were written.
int TextLayoutRangeRects(TextLayout* tl, Str s, int u8a, int u8b, Bounds* out,
                         int max);
// Where the baseline sits inside a line box, measured from the line's top.
// The first line's, which is the one a decoration under a run needs: an
// input method composes one line at a time.
float TextLayoutBaseline(TextLayout* tl);

} // namespace gpui
