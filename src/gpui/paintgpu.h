#ifndef GPUI_GPUI_PAINTGPU_H_
#define GPUI_GPUI_PAINTGPU_H_
/* A second Windows backend for Paint.h, which draws the way GPUI's own
   renderer does. It is off by default and exists to be measured against the
   Direct2D one; see the note at the end for what it is worth and what it is
   still short of.

   The default Windows backend is already on the GPU: Direct2D on a D3D11
   device, presenting through a DXGI flip-model swap chain. What it is not is
   GPUI's *shape* of GPU renderer. D2D takes one call per primitive and
   decides for itself how to batch them; Blade — and `directx_renderer.rs`
   behind it — puts every rounded rect, border and glyph of a frame into one
   instance buffer and issues a handful of draws, with the rounding, the
   border and the content mask evaluated analytically in the pixel shader.
   This is that, far enough along to draw the story gallery and the showcase
   and be timed against the D2D path on the same machine in the same process.

   `GPUI_PAINT=gpu` in the environment picks it, read once at startup, so the
   D2D backend stays the default and nothing about a normal build changes.
   `GPUI_PAINT_MSAA=1|2|4|8` sets the sample count (default 4): quads and
   glyphs carry their own analytic coverage, but tessellated paths and
   expanded strokes get their antialiasing from the sample count, and its
   cost is worth being able to see.

   What lives where: everything device-independent is shared with
   paint_win.cpp rather than written twice — the DirectWrite factory and its
   text formats, an IDWriteTextLayout (which is what a `TextLayout*` is on
   Windows, so shaping, measurement, hit-testing and range rects are the same
   code on both paths), the WIC image decode, and the D3D11 device itself.
   Only the target and the drawing differ. */

#include "gpui/paint.h"

#if GPUI_OS_WINDOWS

namespace gpui {

// Read once, from GPUI_PAINT. The backend cannot change while a target is
// alive, and nothing here re-reads it.
bool PaintGpuOn();
// The sample count the GPU backend renders at; 1 is no multisampling.
int PaintGpuSamples();

// ─── borrowed from the D2D backend ───────────────────────────────────────
//
// Implemented in paint_win.cpp, which owns them. They come back as void*
// rather than as their COM types so that this header — which lands in the
// amalgamated gpui.h on every platform — pulls in no D3D or DirectWrite
// headers of its own.

// ID3D11Device*, created on first use. Null if the machine has neither a
// hardware device nor WARP.
void* PaintSharedD3dDevice(PaintApp* pa);
// IDXGIFactory2*, the one the shared device's adapter belongs to.
void* PaintSharedDxgiFactory(PaintApp* pa);
// IDWriteFactory*.
void* PaintSharedDwrite(PaintApp* pa);
// The premultiplied BGRA WIC decoded, which is what ImageDecode keeps. False
// when the image never decoded.
bool PaintImagePixels(const Image* img, const uint8_t** bgra, int* w, int* h);

// ─── the GPU backend ─────────────────────────────────────────────────────
//
// One name for one name with Paint.h, so paint_win.cpp's dispatch is a line
// per entry point and the two implementations never have to agree on
// anything else. Everything Paint.h declares that is *not* here is shared:
// PaintAppNew / Free, ImageDecode / Free / SizePx, and every TextLayout call
// but Draw.

namespace gpuw {

bool PaintTargetBegin(PaintCtx* ctx, void* native, int pxW, int pxH);
bool PaintTargetBeginOffscreen(PaintCtx* ctx, int pxW, int pxH);
bool PaintTargetEndOffscreen(PaintCtx* ctx, uint8_t* outBgra);
bool PaintTargetEnd(PaintCtx* ctx);
void PaintTargetFree(PaintCtx* ctx);

void CanvasClear(PaintCtx* ctx, Rgba c);
void CanvasFillRect(PaintCtx* ctx, float x, float y, float w, float h, Rgba c);
void CanvasFillRound(PaintCtx* ctx, float x, float y, float w, float h, float r,
                     Rgba c);
void CanvasStrokeRound(PaintCtx* ctx, float x, float y, float w, float h,
                       float r, float stroke, Rgba c, const float* dash);
void CanvasLine(PaintCtx* ctx, float x1, float y1, float x2, float y2,
                float stroke, Rgba c, const float* dash);
void CanvasEllipse(PaintCtx* ctx, float cx, float cy, float rx, float ry,
                   float stroke, Rgba c);
void CanvasPushClip(PaintCtx* ctx, float x, float y, float w, float h);
void CanvasPopClip(PaintCtx* ctx);

Path* PathNew(PaintCtx* ctx, bool winding);
void PathFree(Path* p);
void PathMoveTo(Path* p, float x, float y);
void PathLineTo(Path* p, float x, float y);
void PathCubicTo(Path* p, float x1, float y1, float x2, float y2, float x,
                 float y);
void PathArcTo(Path* p, float cx, float cy, float r, float a0, float a1,
               bool clockwise);
void PathClose(Path* p);
void PathFill(PaintCtx* ctx, Path* p, Rgba c);
void PathFillGradient(PaintCtx* ctx, Path* p, float x0, float y0, float x1,
                      float y1, Rgba from, Rgba to);
void PathStroke(PaintCtx* ctx, Path* p, float stroke, Rgba c, bool roundCaps);
void PathRealize(PaintCtx* ctx, Path* p);

void ImageDraw(PaintCtx* ctx, Image* img, Bounds b, float radius);
void TextLayoutDraw(PaintCtx* ctx, TextLayout* tl, float x, float y, Rgba c,
                    bool clip, float clipW);

// What the last frame cost, for the benchmark harness: how many instances the
// frame put in the buffer, how many draw calls it took, and how many glyphs
// had to be rasterized into the atlas rather than found in it.
struct FrameStats {
    int instances = 0;
    int draws = 0;
    int glyphsRasterized = 0;
    int pathTriangles = 0;
};
const FrameStats& LastFrameStats();

} // namespace gpuw

// ─── what it is worth ────────────────────────────────────────────────────
//
// Measured with GPUI_FRAME_BENCH (see window_common.cpp), which times the
// three phases of Window::draw apart. Only the paint phase is below; the
// other two are the same code on both backends. Mean of 800 frames, release,
// one machine — so the ratios are the answer, not the absolute numbers.
//
//     scene             D2D        this
//     showcase          0.21 ms    0.09 ms    2.3x
//     system_monitor    0.46 ms    0.16 ms    2.9x
//     story             1.86 ms    0.99 ms    1.9x
//
// The charts are where it wins biggest, because D2D re-tessellates path
// geometry on the CPU every frame and stencil-and-cover does not. Read the
// whole-frame number before getting excited: `story` spends 5.8 ms in layout,
// so halving the paint is 11% of the frame. Multisampling is close to free on
// the two heavier scenes and costs about 0.05 ms on the light one.
//
// Two things that came out of measuring it, in case either is ever true
// again: the showcase was *slower* than D2D until the per-frame stencil clear
// went (a full-surface D24S8 clear is pure bandwidth, and the cover pass
// already leaves the buffer at zero), and it batched the whole scene into a
// single draw call while doing it, so the cost was never the drawing.
//
// Against the D2D path, pixel for pixel: 2.7% of the story's pixels differ
// and 1.2% of the chart page's, all of it text — DirectWrite draws ClearType
// and the atlas is grayscale — plus the third-of-a-pixel positioning this
// does not do. No geometry moves.

} // namespace gpui

#endif // GPUI_OS_WINDOWS
#endif // GPUI_GPUI_PAINTGPU_H_
