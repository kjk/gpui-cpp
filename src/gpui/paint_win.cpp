/* Direct2D + DirectWrite backend for Paint.h. */

#include "gpui/paint.h"

#include <d2d1.h>
#include <dwrite.h>
#include <math.h>
#include <wincodec.h>

namespace gpui {

static inline D2D1_COLOR_F ToD2D(Rgba c) {
    return D2D1::ColorF(c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f);
}

template <typename T>
static void Rel(T** p) {
    if (p && *p) {
        (*p)->Release();
        *p = nullptr;
    }
}

struct PaintApp {
    ID2D1Factory* d2d = nullptr;
    IDWriteFactory* dwrite = nullptr;
    IDWriteTextFormat* font12 = nullptr;
    IDWriteTextFormat* font14 = nullptr;
    IDWriteTextFormat* font16 = nullptr;
    IDWriteTextFormat* font20 = nullptr;
    IDWriteTextFormat* font24 = nullptr;
    IDWriteTextFormat* fontMono = nullptr;
};

// A DC render target is bound to a fresh HDC every WM_PAINT, so the target
// itself survives across frames and only the binding changes.
struct PaintTarget {
    ID2D1DCRenderTarget* dcRt = nullptr;
    ID2D1RenderTarget* rt = nullptr;
    ID2D1SolidColorBrush* brush = nullptr;
};

// ─── lifecycle ────────────────────────────────────────────────────────────

static void MakeFontFamily(PaintApp* pa, const wchar_t* family, float px,
                           int weight, IDWriteTextFormat** out) {
    DWRITE_FONT_WEIGHT w =
        weight ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL;
    pa->dwrite->CreateTextFormat(family, nullptr, w, DWRITE_FONT_STYLE_NORMAL,
                                 DWRITE_FONT_STRETCH_NORMAL, px, L"en-us", out);
    if (*out) {
        (*out)->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }
}

PaintApp* PaintAppNew() {
    auto* pa = new PaintApp();
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pa->d2d);
    if (FAILED(hr)) {
        delete pa;
        return nullptr;
    }
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                             __uuidof(IDWriteFactory), (IUnknown**)&pa->dwrite);
    if (FAILED(hr)) {
        Rel(&pa->d2d);
        delete pa;
        return nullptr;
    }
    MakeFontFamily(pa, L"Segoe UI", 12.f, 0, &pa->font12);
    MakeFontFamily(pa, L"Segoe UI", 14.f, 0, &pa->font14);
    MakeFontFamily(pa, L"Segoe UI", 16.f, 0, &pa->font16);
    MakeFontFamily(pa, L"Segoe UI", 20.f, 1, &pa->font20);
    MakeFontFamily(pa, L"Segoe UI", 24.f, 1, &pa->font24);
    // The monospace family the FPS HUD asks for. Consolas ships with Windows,
    // so nothing has to configure a font for its columns to line up.
    MakeFontFamily(pa, L"Consolas", 12.f, 0, &pa->fontMono);
    return pa;
}

void PaintAppFree(PaintApp* pa) {
    if (!pa) {
        return;
    }
    Rel(&pa->font12);
    Rel(&pa->font14);
    Rel(&pa->font16);
    Rel(&pa->font20);
    Rel(&pa->font24);
    Rel(&pa->fontMono);
    Rel(&pa->dwrite);
    Rel(&pa->d2d);
    delete pa;
}

void PaintTargetFree(PaintCtx* ctx) {
    if (!ctx || !ctx->rt) {
        return;
    }
    Rel(&ctx->rt->brush);
    Rel(&ctx->rt->dcRt);
    delete ctx->rt;
    ctx->rt = nullptr;
}

bool PaintTargetBegin(PaintCtx* ctx, void* native, int pxW, int pxH) {
    if (!ctx || !ctx->pa) {
        return false;
    }
    if (!ctx->rt) {
        auto* t = new PaintTarget();
        D2D1_RENDER_TARGET_PROPERTIES rtp = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                              D2D1_ALPHA_MODE_IGNORE),
            96.f, 96.f);
        HRESULT hr = ctx->pa->d2d->CreateDCRenderTarget(&rtp, &t->dcRt);
        if (FAILED(hr)) {
            logf("CreateDCRenderTarget failed %08x", (unsigned)hr);
            delete t;
            return false;
        }
        t->rt = t->dcRt;
        hr = t->rt->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1), &t->brush);
        if (FAILED(hr)) {
            Rel(&t->dcRt);
            delete t;
            return false;
        }
        ctx->rt = t;
    }
    RECT rc = {0, 0, pxW, pxH};
    HRESULT hr = ctx->rt->dcRt->BindDC((HDC)native, &rc);
    if (FAILED(hr)) {
        logf("BindDC failed %08x", (unsigned)hr);
        PaintTargetFree(ctx);
        return false;
    }
    ctx->rt->rt->BeginDraw();
    ctx->rt->rt->SetTransform(D2D1::Matrix3x2F::Identity());
    return true;
}

// The offscreen target, which the window target is not: a DIB the render
// target draws into with its alpha kept, so what comes out can be blended
// over whatever the OS puts behind it.
struct OffscreenTarget {
    HBITMAP bmp = nullptr;
    HDC dc = nullptr;
    HGDIOBJ oldBmp = nullptr;
    void* bits = nullptr;
    int w = 0;
    int h = 0;
};
static OffscreenTarget gOffscreen;

bool PaintTargetBeginOffscreen(PaintCtx* ctx, int pxW, int pxH) {
    if (!ctx || !ctx->pa || pxW <= 0 || pxH <= 0) {
        return false;
    }
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = pxW;
    // Negative: the rows run top down, the way every caller reads them.
    bi.bmiHeader.biHeight = -pxH;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp =
        CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp || !bits) {
        return false;
    }
    memset(bits, 0, (size_t)pxW * (size_t)pxH * 4);
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) {
        DeleteObject(bmp);
        return false;
    }
    HGDIOBJ oldBmp = SelectObject(dc, bmp);

    // A target of its own: the window's ignores alpha, and this one must not.
    PaintTargetFree(ctx);
    auto* t = new PaintTarget();
    D2D1_RENDER_TARGET_PROPERTIES rtp = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                          D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.f, 96.f);
    HRESULT hr = ctx->pa->d2d->CreateDCRenderTarget(&rtp, &t->dcRt);
    if (SUCCEEDED(hr)) {
        t->rt = t->dcRt;
        hr = t->rt->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1), &t->brush);
    }
    if (FAILED(hr)) {
        Rel(&t->brush);
        Rel(&t->dcRt);
        delete t;
        SelectObject(dc, oldBmp);
        DeleteDC(dc);
        DeleteObject(bmp);
        return false;
    }
    ctx->rt = t;
    RECT rc = {0, 0, pxW, pxH};
    hr = t->dcRt->BindDC(dc, &rc);
    if (FAILED(hr)) {
        PaintTargetFree(ctx);
        SelectObject(dc, oldBmp);
        DeleteDC(dc);
        DeleteObject(bmp);
        return false;
    }
    gOffscreen.bmp = bmp;
    gOffscreen.dc = dc;
    gOffscreen.oldBmp = oldBmp;
    gOffscreen.bits = bits;
    gOffscreen.w = pxW;
    gOffscreen.h = pxH;
    t->rt->BeginDraw();
    t->rt->SetTransform(D2D1::Matrix3x2F::Identity());
    t->rt->Clear(D2D1::ColorF(0, 0.f));
    return true;
}

bool PaintTargetEndOffscreen(PaintCtx* ctx, uint8_t* outBgra) {
    if (!ctx || !ctx->rt || !gOffscreen.bmp) {
        return false;
    }
    HRESULT hr = ctx->rt->rt->EndDraw();
    bool ok = SUCCEEDED(hr);
    if (ok && outBgra) {
        // The render target wrote through the DC; GDI has to be flushed
        // before the DIB's own bytes are read.
        GdiFlush();
        memcpy(outBgra, gOffscreen.bits,
               (size_t)gOffscreen.w * (size_t)gOffscreen.h * 4);
    }
    PaintTargetFree(ctx);
    SelectObject(gOffscreen.dc, gOffscreen.oldBmp);
    DeleteDC(gOffscreen.dc);
    DeleteObject(gOffscreen.bmp);
    gOffscreen = OffscreenTarget{};
    return ok;
}

bool PaintTargetEnd(PaintCtx* ctx) {
    if (!ctx || !ctx->rt) {
        return false;
    }
    HRESULT hr = ctx->rt->rt->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        PaintTargetFree(ctx);
        return false;
    }
    return true;
}

// ─── canvas ───────────────────────────────────────────────────────────────

static ID2D1SolidColorBrush* Brush(PaintCtx* ctx, Rgba c) {
    if (!ctx || !ctx->rt || !ctx->rt->brush) {
        return nullptr;
    }
    // element_opacity: every colour the backend is handed is faded by the
    // opacity in force, which is the one place all of them pass through.
    ctx->rt->brush->SetColor(ToD2D(PaintFade(ctx, c)));
    return ctx->rt->brush;
}

// A dashed stroke style, or null for a solid one. Caller releases.
static ID2D1StrokeStyle* DashStyle(PaintCtx* ctx, const float* dash,
                                   bool roundCaps) {
    if (!ctx || !ctx->pa) {
        return nullptr;
    }
    if (!dash && !roundCaps) {
        return nullptr;
    }
    D2D1_STROKE_STYLE_PROPERTIES sp = D2D1::StrokeStyleProperties();
    if (roundCaps) {
        sp.startCap = D2D1_CAP_STYLE_ROUND;
        sp.endCap = D2D1_CAP_STYLE_ROUND;
        sp.dashCap = D2D1_CAP_STYLE_ROUND;
        sp.lineJoin = D2D1_LINE_JOIN_ROUND;
    }
    ID2D1StrokeStyle* ss = nullptr;
    if (dash) {
        sp.dashStyle = D2D1_DASH_STYLE_CUSTOM;
        ctx->pa->d2d->CreateStrokeStyle(sp, dash, 2, &ss);
    } else {
        ctx->pa->d2d->CreateStrokeStyle(sp, nullptr, 0, &ss);
    }
    return ss;
}

void CanvasClear(PaintCtx* ctx, Rgba c) {
    if (ctx && ctx->rt) {
        ctx->rt->rt->Clear(ToD2D(c));
    }
}

void CanvasFillRect(PaintCtx* ctx, float x, float y, float w, float h, Rgba c) {
    if (w <= 0 || h <= 0 || c.a == 0) {
        return;
    }
    ID2D1SolidColorBrush* b = Brush(ctx, c);
    if (b) {
        ctx->rt->rt->FillRectangle(D2D1::RectF(x, y, x + w, y + h), b);
    }
}

void CanvasFillRound(PaintCtx* ctx, float x, float y, float w, float h, float r,
                     Rgba c) {
    if (w <= 0 || h <= 0 || c.a == 0) {
        return;
    }
    ID2D1SolidColorBrush* b = Brush(ctx, c);
    if (!b) {
        return;
    }
    D2D1_ROUNDED_RECT rr;
    rr.rect = D2D1::RectF(x, y, x + w, y + h);
    rr.radiusX = r;
    rr.radiusY = r;
    ctx->rt->rt->FillRoundedRectangle(rr, b);
}

void CanvasStrokeRound(PaintCtx* ctx, float x, float y, float w, float h,
                       float r, float stroke, Rgba c, const float* dash) {
    if (stroke <= 0 || w <= 0 || h <= 0) {
        return;
    }
    ID2D1SolidColorBrush* b = Brush(ctx, c);
    if (!b) {
        return;
    }
    // D2D centers a stroke on its path, so drawing on the bounds would split a
    // hairline across two pixel rows.
    D2D1_ROUNDED_RECT rr;
    rr.rect = D2D1::RectF(x + stroke * 0.5f, y + stroke * 0.5f,
                          x + w - stroke * 0.5f, y + h - stroke * 0.5f);
    rr.radiusX = r;
    rr.radiusY = r;
    ID2D1StrokeStyle* ss = DashStyle(ctx, dash, false);
    ctx->rt->rt->DrawRoundedRectangle(rr, b, stroke, ss);
    Rel(&ss);
}

void CanvasLine(PaintCtx* ctx, float x1, float y1, float x2, float y2,
                float stroke, Rgba c, const float* dash) {
    ID2D1SolidColorBrush* b = Brush(ctx, c);
    if (!b) {
        return;
    }
    ID2D1StrokeStyle* ss = DashStyle(ctx, dash, false);
    ctx->rt->rt
        ->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), b, stroke, ss);
    Rel(&ss);
}

void CanvasEllipse(PaintCtx* ctx, float cx, float cy, float rx, float ry,
                   float stroke, Rgba c) {
    ID2D1SolidColorBrush* b = Brush(ctx, c);
    if (!b) {
        return;
    }
    D2D1_ELLIPSE e = D2D1::Ellipse(D2D1::Point2F(cx, cy), rx, ry);
    if (stroke > 0) {
        ctx->rt->rt->DrawEllipse(e, b, stroke);
    } else {
        ctx->rt->rt->FillEllipse(e, b);
    }
}

void CanvasPushClip(PaintCtx* ctx, float x, float y, float w, float h) {
    if (ctx && ctx->rt) {
        ctx->rt->rt->PushAxisAlignedClip(D2D1::RectF(x, y, x + w, y + h),
                                         D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    }
}

void CanvasPopClip(PaintCtx* ctx) {
    if (ctx && ctx->rt) {
        ctx->rt->rt->PopAxisAlignedClip();
    }
}

// ─── paths ────────────────────────────────────────────────────────────────

struct Path {
    ID2D1PathGeometry* geom = nullptr;
    ID2D1GeometrySink* sink = nullptr;
    bool fig = false; // a figure is open
    bool sealed = false;
    float mx = 0, my = 0; // where the open figure started
};

Path* PathNew(PaintCtx* ctx, bool winding) {
    if (!ctx || !ctx->pa) {
        return nullptr;
    }
    auto* p = new Path();
    if (FAILED(ctx->pa->d2d->CreatePathGeometry(&p->geom)) || !p->geom) {
        delete p;
        return nullptr;
    }
    if (FAILED(p->geom->Open(&p->sink)) || !p->sink) {
        Rel(&p->geom);
        delete p;
        return nullptr;
    }
    p->sink->SetFillMode(winding ? D2D1_FILL_MODE_WINDING
                                 : D2D1_FILL_MODE_ALTERNATE);
    return p;
}

void PathFree(Path* p) {
    if (!p) {
        return;
    }
    if (p->sink) {
        if (p->fig) {
            p->sink->EndFigure(D2D1_FIGURE_END_OPEN);
        }
        if (!p->sealed) {
            p->sink->Close();
        }
        p->sink->Release();
    }
    Rel(&p->geom);
    delete p;
}

void PathMoveTo(Path* p, float x, float y) {
    if (!p || !p->sink) {
        return;
    }
    if (p->fig) {
        p->sink->EndFigure(D2D1_FIGURE_END_OPEN);
    }
    // Always FILLED: a HOLLOW figure is skipped by FillGeometry, and a caller
    // that only strokes never asks for a fill anyway.
    p->sink->BeginFigure(D2D1::Point2F(x, y), D2D1_FIGURE_BEGIN_FILLED);
    p->fig = true;
    p->mx = x;
    p->my = y;
}

void PathLineTo(Path* p, float x, float y) {
    if (!p || !p->sink) {
        return;
    }
    if (!p->fig) {
        PathMoveTo(p, x, y);
        return;
    }
    p->sink->AddLine(D2D1::Point2F(x, y));
}

void PathCubicTo(Path* p, float x1, float y1, float x2, float y2, float x,
                 float y) {
    if (!p || !p->sink) {
        return;
    }
    if (!p->fig) {
        PathMoveTo(p, x, y);
        return;
    }
    D2D1_BEZIER_SEGMENT b;
    b.point1 = D2D1::Point2F(x1, y1);
    b.point2 = D2D1::Point2F(x2, y2);
    b.point3 = D2D1::Point2F(x, y);
    p->sink->AddBezier(b);
}

void PathArcTo(Path* p, float cx, float cy, float r, float a0, float a1,
               bool clockwise) {
    if (!p || !p->sink) {
        return;
    }
    float sx = cx + r * cosf(a0);
    float sy = cy + r * sinf(a0);
    float ex = cx + r * cosf(a1);
    float ey = cy + r * sinf(a1);
    if (!p->fig) {
        PathMoveTo(p, sx, sy);
    } else {
        p->sink->AddLine(D2D1::Point2F(sx, sy));
    }
    float sweep = a1 - a0;
    if (sweep < 0) {
        sweep = -sweep;
    }
    D2D1_ARC_SEGMENT arc = {};
    arc.point = D2D1::Point2F(ex, ey);
    arc.size = D2D1::SizeF(r, r);
    arc.rotationAngle = 0;
    arc.sweepDirection = clockwise ? D2D1_SWEEP_DIRECTION_CLOCKWISE
                                   : D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;
    arc.arcSize =
        sweep > 3.14159265f ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL;
    p->sink->AddArc(arc);
}

void PathClose(Path* p) {
    if (!p || !p->sink || !p->fig) {
        return;
    }
    p->sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    p->fig = false;
}

// Finish building so the geometry can be drawn. Idempotent.
static ID2D1PathGeometry* PathSeal(Path* p) {
    if (!p || !p->geom) {
        return nullptr;
    }
    if (!p->sealed) {
        if (p->fig) {
            p->sink->EndFigure(D2D1_FIGURE_END_OPEN);
            p->fig = false;
        }
        p->sink->Close();
        p->sealed = true;
    }
    return p->geom;
}

void PathFill(PaintCtx* ctx, Path* p, Rgba c) {
    ID2D1PathGeometry* g = PathSeal(p);
    ID2D1SolidColorBrush* b = Brush(ctx, c);
    if (g && b) {
        ctx->rt->rt->FillGeometry(g, b, nullptr);
    }
}

void PathFillGradientV(PaintCtx* ctx, Path* p, float y0, float y1, Rgba top,
                       Rgba bot) {
    PathFillGradient(ctx, p, 0, y0, 0, y1, top, bot);
}

void PathFillGradient(PaintCtx* ctx, Path* p, float x0, float y0, float x1,
                      float y1, Rgba from, Rgba to) {
    ID2D1PathGeometry* g = PathSeal(p);
    if (!g || !ctx || !ctx->rt) {
        return;
    }
    D2D1_GRADIENT_STOP gs[2];
    gs[0].position = 0.f;
    gs[0].color = ToD2D(PaintFade(ctx, from));
    gs[1].position = 1.f;
    gs[1].color = ToD2D(PaintFade(ctx, to));
    ID2D1GradientStopCollection* stops = nullptr;
    ctx->rt->rt->CreateGradientStopCollection(gs, 2, &stops);
    bool filled = false;
    if (stops) {
        ID2D1LinearGradientBrush* gb = nullptr;
        ctx->rt->rt->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(D2D1::Point2F(x0, y0),
                                                D2D1::Point2F(x1, y1)),
            stops, &gb);
        if (gb) {
            ctx->rt->rt->FillGeometry(g, gb);
            gb->Release();
            filled = true;
        }
        stops->Release();
    }
    if (!filled) {
        PathFill(ctx, p, from);
    }
}

void PathStroke(PaintCtx* ctx, Path* p, float stroke, Rgba c, bool roundCaps) {
    ID2D1PathGeometry* g = PathSeal(p);
    ID2D1SolidColorBrush* b = Brush(ctx, c);
    if (!g || !b) {
        return;
    }
    ID2D1StrokeStyle* ss = DashStyle(ctx, nullptr, roundCaps);
    ctx->rt->rt->DrawGeometry(g, b, stroke, ss);
    Rel(&ss);
}

// ─── shaped text ──────────────────────────────────────────────────────────

static IDWriteTextFormat* FontFor(PaintApp* pa, float fontSize,
                                  uint8_t weight) {
    if ((weight & kFontMono) && pa->fontMono) {
        return pa->fontMono;
    }
    if (fontSize >= 22.f && pa->font24) {
        return pa->font24;
    }
    if (fontSize >= 18.f && pa->font20) {
        return pa->font20;
    }
    if (fontSize <= 13.f) {
        return pa->font12;
    }
    if (fontSize <= 15.f) {
        return pa->font14;
    }
    return pa->font16;
}

static DWRITE_FONT_WEIGHT DwriteWeight(uint8_t weight) {
    switch (weight & kFontWeightMask) {
        case kFontWeightBold:
            return DWRITE_FONT_WEIGHT_BOLD;
        case kFontWeightSemibold:
            return DWRITE_FONT_WEIGHT_SEMI_BOLD;
        case kFontWeightMedium:
            return DWRITE_FONT_WEIGHT_MEDIUM;
        default:
            return DWRITE_FONT_WEIGHT_NORMAL;
    }
}

static int Utf8ToWideN(Str s, WCHAR* wbuf, int cap) {
    if (!s.s || s.len <= 0 || cap < 2) {
        if (wbuf && cap > 0) {
            wbuf[0] = 0;
        }
        return 0;
    }
    int n = MultiByteToWideChar(CP_UTF8, 0, s.s, s.len, wbuf, cap - 1);
    if (n < 0) {
        n = 0;
    }
    wbuf[n] = 0;
    return n;
}

static int Utf8OffToWide(Str s, int u8off) {
    if (u8off <= 0 || !s.s) {
        return 0;
    }
    if (u8off > s.len) {
        u8off = s.len;
    }
    return MultiByteToWideChar(CP_UTF8, 0, s.s, u8off, nullptr, 0);
}

static int WideOffToUtf8(Str s, int woff) {
    if (woff <= 0 || !s.s) {
        return 0;
    }
    WCHAR wbuf[2048];
    int wn = Utf8ToWideN(s, wbuf, 2048);
    if (woff > wn) {
        woff = wn;
    }
    return WideCharToMultiByte(CP_UTF8, 0, wbuf, woff, nullptr, 0, nullptr,
                               nullptr);
}

// ─── images ───────────────────────────────────────────────────────────────
//
// WIC decodes whatever the machine has a codec for — PNG, JPEG, GIF, BMP,
// TIFF, and HEIC / WebP where Windows ships them — into premultiplied BGRA,
// which is the one format D2D takes without conversion. The pixels are kept
// rather than the D2D bitmap: a DC render target is recreated on a resize or
// a lost device, so the bitmap is rebuilt beside it when the target it was
// made for is no longer the one being drawn into.

struct Image {
    int w = 0;
    int h = 0;
    uint8_t* bgra = nullptr;
    ID2D1Bitmap* bmp = nullptr;
    // The render target `bmp` belongs to. D2D bitmaps are device resources
    // and do not survive it.
    ID2D1RenderTarget* bmpRt = nullptr;
};

Image* ImageDecode(PaintApp* pa, const uint8_t* bytes, int len) {
    (void)pa;
    if (!bytes || len <= 0) {
        return nullptr;
    }
    IWICImagingFactory* wic = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic));
    if (FAILED(hr) || !wic) {
        return nullptr;
    }
    IWICStream* stream = nullptr;
    IWICBitmapDecoder* dec = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* conv = nullptr;
    Image* img = nullptr;
    hr = wic->CreateStream(&stream);
    if (SUCCEEDED(hr)) {
        hr = stream->InitializeFromMemory((BYTE*)bytes, (DWORD)len);
    }
    if (SUCCEEDED(hr)) {
        hr = wic->CreateDecoderFromStream(stream, nullptr,
                                          WICDecodeMetadataCacheOnLoad, &dec);
    }
    if (SUCCEEDED(hr)) {
        hr = dec->GetFrame(0, &frame);
    }
    if (SUCCEEDED(hr)) {
        hr = wic->CreateFormatConverter(&conv);
    }
    if (SUCCEEDED(hr)) {
        hr = conv->Initialize(frame, GUID_WICPixelFormat32bppPBGRA,
                              WICBitmapDitherTypeNone, nullptr, 0.f,
                              WICBitmapPaletteTypeMedianCut);
    }
    UINT w = 0, h = 0;
    if (SUCCEEDED(hr)) {
        hr = conv->GetSize(&w, &h);
    }
    if (SUCCEEDED(hr) && w > 0 && h > 0) {
        UINT stride = w * 4;
        UINT size = stride * h;
        auto* px = (uint8_t*)Alloc(nullptr, (int)size);
        if (px && SUCCEEDED(conv->CopyPixels(nullptr, stride, size, px))) {
            img = new Image();
            img->w = (int)w;
            img->h = (int)h;
            img->bgra = px;
        } else {
            Free(nullptr, px);
        }
    }
    Rel(&conv);
    Rel(&frame);
    Rel(&dec);
    Rel(&stream);
    Rel(&wic);
    return img;
}

void ImageFree(Image* img) {
    if (!img) {
        return;
    }
    Rel(&img->bmp);
    Free(nullptr, img->bgra);
    delete img;
}

Size ImageSizePx(const Image* img) {
    if (!img) {
        return {};
    }
    return {(float)img->w, (float)img->h};
}

void ImageDraw(PaintCtx* ctx, Image* img, Bounds b) {
    if (!ctx || !ctx->rt || !ctx->rt->rt || !img || !img->bgra || b.w <= 0 ||
        b.h <= 0) {
        return;
    }
    ID2D1RenderTarget* rt = ctx->rt->rt;
    if (img->bmp && img->bmpRt != rt) {
        Rel(&img->bmp);
    }
    if (!img->bmp) {
        D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(D2D1::PixelFormat(
            DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        D2D1_SIZE_U size = D2D1::SizeU((UINT32)img->w, (UINT32)img->h);
        UINT32 pitch = (UINT32)img->w * 4;
        HRESULT hr = rt->CreateBitmap(size, img->bgra, pitch, props, &img->bmp);
        if (FAILED(hr) || !img->bmp) {
            return;
        }
        img->bmpRt = rt;
    }
    D2D1_RECT_F dst = D2D1::RectF(b.x, b.y, b.x + b.w, b.y + b.h);
    rt->DrawBitmap(img->bmp, dst, ctx->opacity < 0 ? 0.f : ctx->opacity,
                   D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
}

// gpui text_system: padding_top = (line_height - ascent - descent) / 2.
static void ApplyLineHeight(IDWriteTextLayout* layout, float fontSize,
                            float mult) {
    if (!layout || fontSize <= 0) {
        return;
    }
    DWRITE_LINE_METRICS lm = {};
    UINT32 n = 0;
    // Returns E_NOT_SUFFICIENT_BUFFER past the first line, which still fills
    // it; every line has the same metrics here, so one is enough.
    layout->GetLineMetrics(&lm, 1, &n);
    if (n == 0 || lm.height <= 0) {
        return;
    }
    float box = fontSize * (mult > 0 ? mult : kLineHeight);
    float baseline = lm.baseline + (box - lm.height) * 0.5f;
    layout->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, box, baseline);
}

static IDWriteTextLayout* Dw(TextLayout* tl) {
    return (IDWriteTextLayout*)tl;
}

TextLayout* TextLayoutNew(PaintCtx* ctx, Str s, float fontSize, float maxW,
                          bool wrap, uint8_t weight, float lineH,
                          Size* outSize) {
    if (!ctx || !ctx->pa || !ctx->pa->dwrite || !s.s || s.len <= 0) {
        return nullptr;
    }
    IDWriteTextFormat* fmt = FontFor(ctx->pa, fontSize, weight);
    if (!fmt) {
        return nullptr;
    }
    WCHAR wbuf[2048];
    int n = Utf8ToWideN(s, wbuf, 2048);
    if (n <= 0) {
        return nullptr;
    }
    IDWriteTextLayout* layout = nullptr;
    float layoutW = maxW > 0 ? maxW : 10000.f;
    HRESULT hr = ctx->pa->dwrite->CreateTextLayout(wbuf, (UINT32)n, fmt,
                                                   layoutW, 4000.f, &layout);
    if (FAILED(hr) || !layout) {
        return nullptr;
    }
    DWRITE_TEXT_RANGE range = {0, (UINT32)n};
    if (fontSize > 0) {
        layout->SetFontSize(fontSize, range);
    }
    if (weight & kFontWeightMask) {
        layout->SetFontWeight(DwriteWeight(weight), range);
    }
    if (weight & kFontUnderline) {
        layout->SetUnderline(TRUE, range);
    }
    if (weight & kFontStrike) {
        layout->SetStrikethrough(TRUE, range);
    }
    if (weight & kFontItalic) {
        layout->SetFontStyle(DWRITE_FONT_STYLE_ITALIC, range);
    }
    layout->SetWordWrapping(wrap && maxW > 0 ? DWRITE_WORD_WRAPPING_WRAP
                                             : DWRITE_WORD_WRAPPING_NO_WRAP);
    ApplyLineHeight(layout, fontSize, lineH);
    DWRITE_TEXT_METRICS m = {};
    layout->GetMetrics(&m);
    if (outSize) {
        outSize->w = m.widthIncludingTrailingWhitespace;
        outSize->h = m.height;
    }
    return (TextLayout*)layout;
}

void TextLayoutAddRef(TextLayout* tl) {
    if (tl) {
        Dw(tl)->AddRef();
    }
}

void TextLayoutRelease(TextLayout* tl) {
    if (tl) {
        Dw(tl)->Release();
    }
}

void TextLayoutDraw(PaintCtx* ctx, TextLayout* tl, float x, float y, Rgba c,
                    bool clip) {
    ID2D1SolidColorBrush* b = Brush(ctx, c);
    if (!tl || !b) {
        return;
    }
    D2D1_DRAW_TEXT_OPTIONS opt =
        clip ? D2D1_DRAW_TEXT_OPTIONS_CLIP : D2D1_DRAW_TEXT_OPTIONS_NONE;
    ctx->rt->rt->DrawTextLayout(D2D1::Point2F(x, y), Dw(tl), b, opt);
}

int TextLayoutHitPoint(TextLayout* tl, Str s, float relX, float relY) {
    if (!tl) {
        return 0;
    }
    WCHAR wbuf[2048];
    int wn = Utf8ToWideN(s, wbuf, 2048);
    BOOL trailing = FALSE;
    BOOL inside = FALSE;
    DWRITE_HIT_TEST_METRICS m = {};
    Dw(tl)->HitTestPoint(relX, relY, &trailing, &inside, &m);
    int wpos = (int)m.textPosition;
    if (trailing) {
        wpos += (int)m.length;
    }
    if (wpos < 0) {
        wpos = 0;
    }
    if (wpos > wn) {
        wpos = wn;
    }
    return WideOffToUtf8(s, wpos);
}

float TextLayoutBaseline(TextLayout* tl) {
    if (!tl) {
        return 0;
    }
    DWRITE_LINE_METRICS line = {};
    UINT32 actual = 0;
    if (FAILED(Dw(tl)->GetLineMetrics(&line, 1, &actual)) || actual == 0) {
        return 0;
    }
    return (float)line.baseline;
}

int TextLayoutRangeRects(TextLayout* tl, Str s, int u8a, int u8b, Bounds* out,
                         int max) {
    if (!tl || !out || max <= 0 || u8a >= u8b) {
        return 0;
    }
    IDWriteTextLayout* layout = Dw(tl);
    int wa = Utf8OffToWide(s, u8a);
    int wb = Utf8OffToWide(s, u8b);
    if (wa > wb) {
        int t = wa;
        wa = wb;
        wb = t;
    }
    DWRITE_TEXT_METRICS tm = {};
    layout->GetMetrics(&tm);
    UINT32 lineCount = tm.lineCount;
    if (lineCount == 0) {
        return 0;
    }
    DWRITE_LINE_METRICS lines[32] = {};
    if (lineCount > 32) {
        lineCount = 32;
    }
    UINT32 actual = 0;
    layout->GetLineMetrics(lines, lineCount, &actual);
    UINT32 pos = 0;
    int n = 0;
    for (UINT32 i = 0; i < actual && n < max; i++) {
        int lineStart = (int)pos;
        int lineEnd = lineStart + (int)lines[i].length;
        int visEnd = lineEnd - (int)lines[i].newlineLength;
        pos = (UINT32)lineEnd;
        int lo = wa > lineStart ? wa : lineStart;
        int hi = wb < visEnd ? wb : visEnd;
        if (lo >= hi) {
            continue;
        }
        FLOAT x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        DWRITE_HIT_TEST_METRICS a = {}, b = {};
        layout->HitTestTextPosition((UINT32)lo, FALSE, &x0, &y0, &a);
        layout->HitTestTextPosition((UINT32)hi, FALSE, &x1, &y1, &b);
        float left = x0;
        float right = x1;
        if (right < left) {
            float tmp = left;
            left = right;
            right = tmp;
        }
        // A selection that runs to the end of a wrapped line covers the rest
        // of the line box, the way a text editor draws it.
        if (hi == visEnd && lo < visEnd) {
            right = tm.layoutWidth;
            if (x1 > 0 && x1 + 1.f < tm.layoutWidth) {
                right = x1;
            }
        }
        out[n].x = left;
        out[n].y = y0;
        out[n].w = right - left;
        out[n].h = lines[i].height;
        n++;
    }
    return n;
}

} // namespace gpui
