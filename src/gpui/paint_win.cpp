/* Windows Paint.h front end and the Direct2D backend. DirectWrite shaping and
   WIC decode are shared with the fixed D3D11/D3D12 builds too. */

#include "gpui/paint.h"
// The custom GPU renderer beside this one. In WIN_BACKEND_ALL builds,
// __paint=d3d11|d3d12 selects it; fixed custom builds
// always hand every entry point to their one submission half.
#include "gpui/paintgpu.h"
#include "gpui/scene.h"

#include <d2d1.h>
#include <d2d1_1.h>
// ID2D1DeviceContext1 and ID2D1GeometryRealization, which is what makes a
// path cheap to fill twice. Windows 8.1 and up; PathRealize falls back when
// the QueryInterface fails.
#include <d2d1_2.h>
#include <d3d11.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <math.h>
#include <wincodec.h>

namespace gpui {

static WinPaintOptions gWinPaintOptions = {
#if WIN_BACKEND_D3D11 && !WIN_BACKEND_ALL
    WinPaintBackend::D3D11,
#elif WIN_BACKEND_D3D12 && !WIN_BACKEND_ALL
    WinPaintBackend::D3D12,
#else
    WinPaintBackend::Direct2D,
#endif
    WinPaintMsaa::X4,
    WinSceneMode::Skip,
};

const WinPaintOptions& WinPaintOptionsGet() {
    return gWinPaintOptions;
}

static bool WinPaintBackendAvailable(WinPaintBackend backend) {
    return (backend == WinPaintBackend::Direct2D && WIN_BACKEND_DIRECT2D) ||
           (backend == WinPaintBackend::D3D11 && WIN_BACKEND_D3D11) ||
           (backend == WinPaintBackend::D3D12 && WIN_BACKEND_D3D12);
}

bool WinPaintOptionsTakeArg(Str arg) {
    const Str paint = StrL("__paint=");
    if (base::StrStartsWith(arg, paint)) {
        Str value(arg.s + paint.len, arg.len - paint.len);
        WinPaintBackend backend = WinPaintBackend::Direct2D;
        bool valid = true;
        if (base::StrEqI(value, "d2d")) {
            backend = WinPaintBackend::Direct2D;
        } else if (base::StrEqI(value, "d3d11")) {
            backend = WinPaintBackend::D3D11;
        } else if (base::StrEqI(value, "d3d12")) {
            backend = WinPaintBackend::D3D12;
        } else {
            valid = false;
        }
        if (valid && WinPaintBackendAvailable(backend)) {
            gWinPaintOptions.backend = backend;
        }
        return true;
    }

    const Str msaa = StrL("__msaa=");
    if (base::StrStartsWith(arg, msaa)) {
        Str value(arg.s + msaa.len, arg.len - msaa.len);
        if (base::StrEq(value, "1")) {
            gWinPaintOptions.msaa = WinPaintMsaa::X1;
        } else if (base::StrEq(value, "2")) {
            gWinPaintOptions.msaa = WinPaintMsaa::X2;
        } else if (base::StrEq(value, "4")) {
            gWinPaintOptions.msaa = WinPaintMsaa::X4;
        } else if (base::StrEq(value, "8")) {
            gWinPaintOptions.msaa = WinPaintMsaa::X8;
        }
        return true;
    }

    const Str scene = StrL("__scene=");
    if (!base::StrStartsWith(arg, scene)) {
        return false;
    }
    Str value(arg.s + scene.len, arg.len - scene.len);
    if (base::StrEqI(value, "off")) {
        gWinPaintOptions.scene = WinSceneMode::Off;
    } else if (base::StrEqI(value, "replay")) {
        gWinPaintOptions.scene = WinSceneMode::Replay;
    } else if (base::StrEqI(value, "cache")) {
        gWinPaintOptions.scene = WinSceneMode::Cache;
    } else if (base::StrEqI(value, "skip")) {
        gWinPaintOptions.scene = WinSceneMode::Skip;
    } else if (base::StrEqI(value, "damage")) {
        gWinPaintOptions.scene = WinSceneMode::Damage;
    }
    return true;
}

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
    ID2D1Factory1* d2d = nullptr;
    // The D3D11 device the swap chains and the D2D device context all share.
    // Made lazily, with the first window, so a run that only ever paints
    // offscreen never pays for it.
    ID3D11Device* d3d = nullptr;
    IDXGIDevice1* dxgi = nullptr;
    IDXGIFactory2* dxgiFactory = nullptr;
    ID2D1Device* d2dDevice = nullptr;
    IDWriteFactory* dwrite = nullptr;
    IDWriteTextFormat* font12 = nullptr;
    IDWriteTextFormat* font14 = nullptr;
    IDWriteTextFormat* font16 = nullptr;
    IDWriteTextFormat* font20 = nullptr;
    IDWriteTextFormat* font24 = nullptr;
    IDWriteTextFormat* fontMono = nullptr;
};

// The window target is a DXGI flip-model swap chain with a D2D device context
// drawing into its back buffer, which is the shape GPUI's own Windows renderer
// has. The DC render target it used to be looks simpler — bind a fresh HDC
// every WM_PAINT and let GDI blit — but D2D has no GPU path to an HDC: BindDC
// and EndDraw each map a staging texture through the DXGI/GDI interop and copy
// the whole surface across the bus, which profiling (winperf against the
// fps_monitor example) showed costing ~70% of the frame.
//
// Not ID2D1HwndRenderTarget either, which would have been the one-line
// version: its blt-model presentation never reaches the redirection surface
// PrintWindow(PW_RENDERFULLCONTENT) reads, so every window came out blank in
// cmd/shot.ts. A flip-model chain composites the way Rust's does and captures.
//
// Only the offscreen target, which has to hand its pixels back as a DIB, still
// uses a DC render target.
struct PaintTarget {
    ID2D1DCRenderTarget* dcRt = nullptr;
    IDXGISwapChain1* swap = nullptr;
    ID2D1DeviceContext* dc = nullptr;
    ID2D1Bitmap1* backBuffer = nullptr;
    HWND hwnd = nullptr;
    int pxW = 0;
    int pxH = 0;
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
    HRESULT hr;
#if WIN_BACKEND_DIRECT2D
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pa->d2d);
    if (FAILED(hr)) {
        delete pa;
        return nullptr;
    }
#endif
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                             __uuidof(IDWriteFactory),
                             (IUnknown**)&pa->dwrite);
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
    Rel(&pa->d2dDevice);
    Rel(&pa->dxgiFactory);
    Rel(&pa->dxgi);
    Rel(&pa->d3d);
    Rel(&pa->d2d);
    delete pa;
}

void PaintTargetFree(PaintCtx* ctx) {
    // Before anything is released: the scene's path cache holds geometry
    // realizations, which belong to the device context about to go.
    if (SceneOn()) {
        scene::Reset();
    }
    if (PaintGpuOn()) {
        gpuw::PaintTargetFree(ctx);
        return;
    }
    if (!ctx || !ctx->rt) {
        return;
    }
    Rel(&ctx->rt->brush);
    Rel(&ctx->rt->dcRt);
    if (ctx->rt->dc) {
        ctx->rt->dc->SetTarget(nullptr);
    }
    Rel(&ctx->rt->backBuffer);
    Rel(&ctx->rt->dc);
    Rel(&ctx->rt->swap);
    delete ctx->rt;
    ctx->rt = nullptr;
}

// The shared D3D11 / D2D device, made once and kept for the process. BGRA
// support is what lets D2D interop with the D3D device at all; the
// single-threaded flag matches the single-threaded D2D factory above.
static bool EnsureDevice(PaintApp* pa) {
#if WIN_BACKEND_DIRECT2D || WIN_BACKEND_D3D11
    if (pa->d3d) {
        return true;
    }
    UINT flags =
        D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_SINGLETHREADED;
    D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_1,
    };
    UINT nLevels = (UINT)(sizeof(levels) / sizeof(levels[0]));
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                   flags, levels, nLevels, D3D11_SDK_VERSION,
                                   &pa->d3d, nullptr, nullptr);
    if (FAILED(hr)) {
        // No hardware device — a VM without a GPU, or a driver that failed to
        // start. WARP draws the same pixels on the CPU rather than nothing.
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags,
                               levels, nLevels, D3D11_SDK_VERSION, &pa->d3d,
                               nullptr, nullptr);
    }
    if (FAILED(hr)) {
        logf("D3D11CreateDevice failed %08x", (unsigned)hr);
        return false;
    }
    hr = pa->d3d->QueryInterface(__uuidof(IDXGIDevice1), (void**)&pa->dxgi);
    if (FAILED(hr)) {
        Rel(&pa->d3d);
        return false;
    }
    // The frame latency is left at DXGI's default of three, which is what
    // GPUI's renderer does. Pinning it to 1 reads like lower latency and
    // measured ~20% slower here: with one frame in flight, Present blocks the
    // draw on the previous frame's scanout instead of letting the next one
    // start.
    IDXGIAdapter* adapter = nullptr;
    if (SUCCEEDED(pa->dxgi->GetAdapter(&adapter)) && adapter) {
        adapter->GetParent(__uuidof(IDXGIFactory2), (void**)&pa->dxgiFactory);
        adapter->Release();
    }
    if (!pa->dxgiFactory) {
        Rel(&pa->dxgi);
        Rel(&pa->d3d);
        return false;
    }
#if WIN_BACKEND_DIRECT2D
    hr = pa->d2d->CreateDevice(pa->dxgi, &pa->d2dDevice);
    if (FAILED(hr)) {
        logf("ID2D1Factory1::CreateDevice failed %08x", (unsigned)hr);
        Rel(&pa->dxgiFactory);
        Rel(&pa->dxgi);
        Rel(&pa->d3d);
        return false;
    }
#endif
    return true;
#else
    (void)pa;
    return false;
#endif
}

// Point the device context at the swap chain's current back buffer. Called
// when the chain is made and again after every resize, since ResizeBuffers
// hands out new surfaces.
static bool BindBackBuffer(PaintTarget* t) {
    Rel(&t->backBuffer);
    IDXGISurface* surface = nullptr;
    HRESULT hr = t->swap
                     ->GetBuffer(0, __uuidof(IDXGISurface), (void**)&surface);
    if (FAILED(hr)) {
        return false;
    }
    D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
        96.f, 96.f);
    hr = t->dc->CreateBitmapFromDxgiSurface(surface, &props, &t->backBuffer);
    surface->Release();
    if (FAILED(hr)) {
        return false;
    }
    t->dc->SetTarget(t->backBuffer);
    return true;
}

bool PaintTargetBegin(PaintCtx* ctx, void* native, int pxW, int pxH) {
    if (PaintGpuOn()) {
        bool ok = gpuw::PaintTargetBegin(ctx, native, pxW, pxH);
        if (ok && SceneOn()) {
            scene::FrameBegin(ctx);
        }
        return ok;
    }
    if (!ctx || !ctx->pa) {
        return false;
    }
    HWND hwnd = (HWND)native;
    if (!hwnd || pxW <= 0 || pxH <= 0) {
        return false;
    }
    if (!EnsureDevice(ctx->pa)) {
        return false;
    }
    // A target belongs to the window it was made for; a window that changed
    // wants a new chain. A size change only needs ResizeBuffers, which keeps
    // the device context and everything cached on it.
    if (ctx->rt && ctx->rt->hwnd != hwnd) {
        PaintTargetFree(ctx);
    }
    if (!ctx->rt) {
        auto* t = new PaintTarget();
        t->hwnd = hwnd;
        DXGI_SWAP_CHAIN_DESC1 desc = {};
        desc.Width = (UINT)pxW;
        desc.Height = (UINT)pxH;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        // Three buffers and FLIP_SEQUENTIAL, matching GPUI's own DirectX
        // renderer (directx_renderer.rs BUFFER_COUNT). Flipping is what puts
        // the frame on the redirection surface DWM composites and
        // PrintWindow(PW_RENDERFULLCONTENT) reads; SEQUENTIAL keeps the back
        // buffer's contents across a present, so a frame that only redraws
        // part of the window still comes out whole.
        desc.BufferCount = 3;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        HRESULT hr = ctx->pa->dxgiFactory->CreateSwapChainForHwnd(
            ctx->pa->d3d, hwnd, &desc, nullptr, nullptr, &t->swap);
        if (FAILED(hr)) {
            logf("CreateSwapChainForHwnd failed %08x", (unsigned)hr);
            delete t;
            return false;
        }
        // The window handles its own resizing; DXGI's Alt+Enter fullscreen
        // would fight the message loop for it.
        ctx->pa->dxgiFactory
            ->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
        hr = ctx->pa->d2dDevice->CreateDeviceContext(
            D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &t->dc);
        if (FAILED(hr)) {
            Rel(&t->swap);
            delete t;
            return false;
        }
        t->dc->SetDpi(96.f, 96.f);
        if (!BindBackBuffer(t)) {
            Rel(&t->dc);
            Rel(&t->swap);
            delete t;
            return false;
        }
        t->rt = t->dc;
        t->pxW = pxW;
        t->pxH = pxH;
        hr = t->rt->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1), &t->brush);
        if (FAILED(hr)) {
            t->dc->SetTarget(nullptr);
            Rel(&t->backBuffer);
            Rel(&t->dc);
            Rel(&t->swap);
            delete t;
            return false;
        }
        ctx->rt = t;
    } else if (ctx->rt->pxW != pxW || ctx->rt->pxH != pxH) {
        PaintTarget* t = ctx->rt;
        t->dc->SetTarget(nullptr);
        Rel(&t->backBuffer);
        HRESULT hr = t->swap->ResizeBuffers(0, (UINT)pxW, (UINT)pxH,
                                            DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr) || !BindBackBuffer(t)) {
            PaintTargetFree(ctx);
            return false;
        }
        t->pxW = pxW;
        t->pxH = pxH;
        // ResizeBuffers hands out new surfaces with nothing in them, so the
        // frame the scene remembers is not what is on this one.
        scene::Invalidate();
    }
    ctx->rt->rt->BeginDraw();
    ctx->rt->rt->SetTransform(D2D1::Matrix3x2F::Identity());
    // The target is open before the recording starts, because the replay at
    // the end of the frame draws into it.
    if (SceneOn()) {
        scene::FrameBegin(ctx);
    }
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

// An offscreen target is not the window's frame, so the recorder steps aside
// for however long one is open. Saved here because the two entry points are
// what bracket it.
static bool gOffscreenWasRecording = false;

bool PaintTargetBeginOffscreen(PaintCtx* ctx, int pxW, int pxH) {
    gOffscreenWasRecording = scene::SuspendBegin();
    if (PaintGpuOn()) {
        return gpuw::PaintTargetBeginOffscreen(ctx, pxW, pxH);
    }
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
    scene::SuspendEnd(gOffscreenWasRecording);
    if (PaintGpuOn()) {
        return gpuw::PaintTargetEndOffscreen(ctx, outBgra);
    }
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

// Close the recording and draw what it collected. Both backends end a frame
// through here, so the scene is replayed once for the two of them.
static bool SceneFinish(PaintCtx* ctx) {
    if (!SceneOn() || !scene::Recording()) {
        return true;
    }
    Bounds damage = {};
    bool draw = scene::FrameEnd(ctx, &damage);
    if (draw) {
        scene::Replay(ctx, &damage);
    }
    return draw;
}

bool PaintTargetEnd(PaintCtx* ctx) {
    SceneFinish(ctx);
    if (PaintGpuOn()) {
        return gpuw::PaintTargetEnd(ctx);
    }
    if (!ctx || !ctx->rt) {
        return false;
    }
    HRESULT hr = ctx->rt->rt->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        PaintTargetFree(ctx);
        return false;
    }
    // A frame the scene found identical to the last one is not presented:
    // what is on screen is already it, and presenting would rotate the
    // buffers under the damage rectangles.
    if (SUCCEEDED(hr) && ctx->rt->swap && !scene::SkipPresent()) {
        // Sync interval 0, the way GPUI's renderer presents. On a flip-model
        // chain that hands the frame straight to DWM, which composites it at
        // the next vblank anyway, so nothing tears — but the draw is not held
        // waiting for the scanout, which is what would show up as frame time.
        HRESULT ph = ctx->rt->swap->Present(0, 0);
        if (ph == DXGI_ERROR_DEVICE_REMOVED || ph == DXGI_ERROR_DEVICE_RESET) {
            PaintTargetFree(ctx);
            return false;
        }
    }
    return true;
}

// ─── borrowed by the GPU backend ─────────────────────────────────────────
//
// paintgpu_win.cpp shares this file's device, its DirectWrite factory and the
// pixels WIC decoded, so that only the drawing is written twice. They come
// back as void* because paintgpu.h lands in the amalgamated gpui.h on every
// platform and must name no D3D or DirectWrite type.

void* PaintSharedD3dDevice(PaintApp* pa) {
    if (!pa || !EnsureDevice(pa)) {
        return nullptr;
    }
    return pa->d3d;
}

void* PaintSharedDxgiFactory(PaintApp* pa) {
    if (!pa || !EnsureDevice(pa)) {
        return nullptr;
    }
    return pa->dxgiFactory;
}

void* PaintSharedDwrite(PaintApp* pa) {
    return pa ? pa->dwrite : nullptr;
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
    if (scene::Recording()) {
        scene::RecClear(ctx, c);
        return;
    }
    if (PaintGpuOn()) {
        gpuw::CanvasClear(ctx, c);
        return;
    }
    if (ctx && ctx->rt) {
        ctx->rt->rt->Clear(ToD2D(c));
    }
}

void CanvasFillRect(PaintCtx* ctx, float x, float y, float w, float h, Rgba c) {
    if (scene::Recording()) {
        scene::RecFillRect(ctx, x, y, w, h, c);
        return;
    }
    if (PaintGpuOn()) {
        gpuw::CanvasFillRect(ctx, x, y, w, h, c);
        return;
    }
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
    if (scene::Recording()) {
        scene::RecFillRound(ctx, x, y, w, h, r, c);
        return;
    }
    if (PaintGpuOn()) {
        gpuw::CanvasFillRound(ctx, x, y, w, h, r, c);
        return;
    }
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
    if (scene::Recording()) {
        scene::RecStrokeRound(ctx, x, y, w, h, r, stroke, c, dash);
        return;
    }
    if (PaintGpuOn()) {
        gpuw::CanvasStrokeRound(ctx, x, y, w, h, r, stroke, c, dash);
        return;
    }
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
    if (scene::Recording()) {
        scene::RecLine(ctx, x1, y1, x2, y2, stroke, c, dash);
        return;
    }
    if (PaintGpuOn()) {
        gpuw::CanvasLine(ctx, x1, y1, x2, y2, stroke, c, dash);
        return;
    }
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
    if (scene::Recording()) {
        scene::RecEllipse(ctx, cx, cy, rx, ry, stroke, c);
        return;
    }
    if (PaintGpuOn()) {
        gpuw::CanvasEllipse(ctx, cx, cy, rx, ry, stroke, c);
        return;
    }
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
    if (scene::Recording()) {
        scene::RecPushClip(ctx, x, y, w, h);
        return;
    }
    if (PaintGpuOn()) {
        gpuw::CanvasPushClip(ctx, x, y, w, h);
        return;
    }
    if (ctx && ctx->rt) {
        ctx->rt->rt->PushAxisAlignedClip(D2D1::RectF(x, y, x + w, y + h),
                                         D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    }
}

void CanvasPopClip(PaintCtx* ctx) {
    if (scene::Recording()) {
        scene::RecPopClip(ctx);
        return;
    }
    if (PaintGpuOn()) {
        gpuw::CanvasPopClip(ctx);
        return;
    }
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
    // A tessellation D2D built once, from PathRealize. FillGeometry
    // tessellates on every call; DrawGeometryRealization does not, which is
    // the whole reason src/gpui/scene.cpp keeps paths across frames.
    ID2D1GeometryRealization* fillReal = nullptr;
    ID2D1GeometryRealization* strokeReal = nullptr;
    // What width `strokeReal` was built for. A stroke of another width is a
    // different tessellation, and there is one slot, so the realization is
    // rebuilt rather than kept per width — the paths this tree strokes keep
    // one width each.
    float strokeRealW = 0;
};

Path* PathNew(PaintCtx* ctx, bool winding) {
    if (scene::Recording()) {
        return scene::RecPathNew(winding);
    }
    if (PaintGpuOn()) {
        return gpuw::PathNew(ctx, winding);
    }
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
    if (scene::Recording()) {
        scene::RecPathFree(p);
        return;
    }
    if (PaintGpuOn()) {
        gpuw::PathFree(p);
        return;
    }
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
    Rel(&p->fillReal);
    Rel(&p->strokeReal);
    Rel(&p->geom);
    delete p;
}

void PathMoveTo(Path* p, float x, float y) {
    if (scene::Recording()) {
        scene::RecPathMoveTo(p, x, y);
        return;
    }
    if (PaintGpuOn()) {
        gpuw::PathMoveTo(p, x, y);
        return;
    }
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
    if (scene::Recording()) {
        scene::RecPathLineTo(p, x, y);
        return;
    }
    if (PaintGpuOn()) {
        gpuw::PathLineTo(p, x, y);
        return;
    }
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
    if (scene::Recording()) {
        scene::RecPathCubicTo(p, x1, y1, x2, y2, x, y);
        return;
    }
    if (PaintGpuOn()) {
        gpuw::PathCubicTo(p, x1, y1, x2, y2, x, y);
        return;
    }
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
    if (scene::Recording()) {
        scene::RecPathArcTo(p, cx, cy, r, a0, a1, clockwise);
        return;
    }
    if (PaintGpuOn()) {
        gpuw::PathArcTo(p, cx, cy, r, a0, a1, clockwise);
        return;
    }
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
    if (scene::Recording()) {
        scene::RecPathClose(p);
        return;
    }
    if (PaintGpuOn()) {
        gpuw::PathClose(p);
        return;
    }
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

// The device context as its 1.1 self, which is what knows about geometry
// realizations. Null on a machine whose D2D predates them, and everything
// below falls back to filling the geometry directly.
static ID2D1DeviceContext1* Dc1(PaintCtx* ctx) {
    if (!ctx || !ctx->rt || !ctx->rt->dc) {
        return nullptr;
    }
    static ID2D1DeviceContext1* cached = nullptr;
    static ID2D1DeviceContext* forDc = nullptr;
    if (forDc != ctx->rt->dc) {
        Rel(&cached);
        forDc = ctx->rt->dc;
        forDc->QueryInterface(__uuidof(ID2D1DeviceContext1), (void**)&cached);
    }
    return cached;
}

void PathRealize(PaintCtx* ctx, Path* p) {
    if (scene::Recording() || PaintGpuOn()) {
        return;
    }
    ID2D1DeviceContext1* dc = Dc1(ctx);
    ID2D1PathGeometry* g = PathSeal(p);
    if (!dc || !g || p->fillReal) {
        return;
    }
    // Identity transform and one DIP to one pixel, which is what this backend
    // draws at; a realization is tessellated for a scale and this is it.
    dc->CreateFilledGeometryRealization(g, D2D1_DEFAULT_FLATTENING_TOLERANCE,
                                        &p->fillReal);
}

void PathFill(PaintCtx* ctx, Path* p, Rgba c) {
    if (scene::Recording()) {
        scene::RecPathFill(ctx, p, c);
        return;
    }
    if (PaintGpuOn()) {
        gpuw::PathFill(ctx, p, c);
        return;
    }
    ID2D1PathGeometry* g = PathSeal(p);
    ID2D1SolidColorBrush* b = Brush(ctx, c);
    if (!g || !b) {
        return;
    }
    if (p->fillReal) {
        ID2D1DeviceContext1* dc = Dc1(ctx);
        if (dc) {
            dc->DrawGeometryRealization(p->fillReal, b);
            return;
        }
    }
    ctx->rt->rt->FillGeometry(g, b, nullptr);
}

void PathFillGradientV(PaintCtx* ctx, Path* p, float y0, float y1, Rgba top,
                       Rgba bot) {
    PathFillGradient(ctx, p, 0, y0, 0, y1, top, bot);
}

void PathFillGradient(PaintCtx* ctx, Path* p, float x0, float y0, float x1,
                      float y1, Rgba from, Rgba to) {
    if (scene::Recording()) {
        scene::RecPathFillGradient(ctx, p, x0, y0, x1, y1, from, to);
        return;
    }
    if (PaintGpuOn()) {
        gpuw::PathFillGradient(ctx, p, x0, y0, x1, y1, from, to);
        return;
    }
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
    if (scene::Recording()) {
        scene::RecPathStroke(ctx, p, stroke, c, roundCaps);
        return;
    }
    if (PaintGpuOn()) {
        gpuw::PathStroke(ctx, p, stroke, c, roundCaps);
        return;
    }
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
        case kFontWeightThin:
            return DWRITE_FONT_WEIGHT_THIN;
        case kFontWeightExtraLight:
            return DWRITE_FONT_WEIGHT_EXTRA_LIGHT;
        case kFontWeightLight:
            return DWRITE_FONT_WEIGHT_LIGHT;
        case kFontWeightExplicitNormal:
            return DWRITE_FONT_WEIGHT_NORMAL;
        case kFontWeightMedium:
            return DWRITE_FONT_WEIGHT_MEDIUM;
        case kFontWeightSemibold:
            return DWRITE_FONT_WEIGHT_SEMI_BOLD;
        case kFontWeightBold:
            return DWRITE_FONT_WEIGHT_BOLD;
        case kFontWeightExtraBold:
            return DWRITE_FONT_WEIGHT_EXTRA_BOLD;
        case kFontWeightBlack:
            return DWRITE_FONT_WEIGHT_BLACK;
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

// The GPU backend makes its own texture out of the same pixels rather than a
// second D2D bitmap.
bool PaintImagePixels(const Image* img, const uint8_t** bgra, int* w, int* h) {
    if (!img || !img->bgra || img->w <= 0 || img->h <= 0) {
        return false;
    }
    if (bgra) {
        *bgra = img->bgra;
    }
    if (w) {
        *w = img->w;
    }
    if (h) {
        *h = img->h;
    }
    return true;
}

Size ImageSizePx(const Image* img) {
    if (!img) {
        return {};
    }
    return {(float)img->w, (float)img->h};
}

void ImageDraw(PaintCtx* ctx, Image* img, Bounds b, float radius) {
    if (scene::Recording()) {
        scene::RecImageDraw(ctx, img, b, radius);
        return;
    }
    if (PaintGpuOn()) {
        gpuw::ImageDraw(ctx, img, b, radius);
        return;
    }
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
    float op = ctx->opacity < 0 ? 0.f : ctx->opacity;
    if (radius > 0) {
        // A bitmap brush scaled onto the box, filling a rounded rect: the
        // corners come out antialiased and nothing has to be clipped.
        D2D1_BITMAP_BRUSH_PROPERTIES bp = D2D1::BitmapBrushProperties(
            D2D1_EXTEND_MODE_CLAMP, D2D1_EXTEND_MODE_CLAMP,
            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        D2D1_BRUSH_PROPERTIES brp = D2D1::BrushProperties(
            op,
            D2D1::Matrix3x2F::Scale(img->w > 0 ? b.w / (float)img->w : 1.f,
                                    img->h > 0 ? b.h / (float)img->h : 1.f) *
                D2D1::Matrix3x2F::Translation(b.x, b.y));
        ID2D1BitmapBrush* brush = nullptr;
        if (SUCCEEDED(rt->CreateBitmapBrush(img->bmp, bp, brp, &brush)) &&
            brush) {
            float r = radius;
            float half = (b.w < b.h ? b.w : b.h) * 0.5f;
            if (r > half) {
                r = half;
            }
            rt->FillRoundedRectangle(D2D1::RoundedRect(dst, r, r), brush);
            Rel(&brush);
            return;
        }
    }
    rt->DrawBitmap(img->bmp, dst, op, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
}

// gpui text_system: padding_top = (line_height - ascent - descent) / 2.
static void ApplyLineHeight(IDWriteTextLayout* layout, float fontSize,
                            float mult) {
    if (!layout || fontSize <= 0) {
        return;
    }
    // Every line of one layout has the same metrics, so only the first is
    // wanted — but DirectWrite answers a buffer shorter than the line count
    // with E_NOT_SUFFICIENT_BUFFER and writes nothing, so a wrapped run has
    // to be asked for in full. Without this the spacing was left alone on
    // exactly the runs that have more than one line, and a wrapped paragraph
    // came out at the font's natural leading instead of GPUI's phi box.
    UINT32 n = 0;
    layout->GetLineMetrics(nullptr, 0, &n);
    if (n == 0) {
        return;
    }
    enum : uint16_t {
        kMaxLines = 256
    };
    if (n > kMaxLines) {
        n = kMaxLines;
    }
    DWRITE_LINE_METRICS lm[kMaxLines] = {};
    UINT32 got = 0;
    if (FAILED(layout->GetLineMetrics(lm, n, &got)) || got == 0 ||
        lm[0].height <= 0) {
        return;
    }
    float box = fontSize * (mult > 0 ? mult : kLineHeight);
    float baseline = lm[0].baseline + (box - lm[0].height) * 0.5f;
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

Size TextLayoutSize(TextLayout* tl) {
    if (!tl) {
        return Size{0, 0};
    }
    DWRITE_TEXT_METRICS m = {};
    if (FAILED(Dw(tl)->GetMetrics(&m))) {
        return Size{0, 0};
    }
    return Size{m.widthIncludingTrailingWhitespace, m.height};
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
                    bool clip, float clipW) {
    if (scene::Recording()) {
        scene::RecTextDraw(ctx, tl, x, y, c, clip, clipW);
        return;
    }
    if (PaintGpuOn()) {
        gpuw::TextLayoutDraw(ctx, tl, x, y, c, clip, clipW);
        return;
    }
    ID2D1SolidColorBrush* b = Brush(ctx, c);
    if (!tl || !b) {
        return;
    }
    // GPUI's `truncate()` is `text_overflow: Ellipsis`, so a run that does not
    // fit ends in one rather than being cut through a glyph. DirectWrite draws
    // it from a trimming sign against the layout's own max width — and a
    // non-wrapping run was shaped unconstrained, so both are set here, on the
    // way in. The layout is drawn once per call and the width is set every
    // time, so a run shared between a truncating cell and an untruncated one
    // is right in both.
    if (clip && clipW > 0) {
        Dw(tl)->SetMaxWidth(clipW);
        DWRITE_TRIMMING trim = {DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
        IDWriteInlineObject* sign = nullptr;
        if (SUCCEEDED(ctx->pa->dwrite
                          ->CreateEllipsisTrimmingSign(Dw(tl), &sign)) &&
            sign) {
            Dw(tl)->SetTrimming(&trim, sign);
            sign->Release();
        }
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
        float left = x0;
        float right;
        if (hi >= visEnd && hi > lineStart) {
            // The position *at* the end of a wrapped line hit-tests to the
            // start of the next one — x of zero, y a line down — so the end
            // of the run is the trailing edge of the last character on this
            // line. Reading it as a leading hit put the right edge left of
            // the left one, and the swap below then painted the whole line.
            layout->HitTestTextPosition((UINT32)(hi - 1), TRUE, &x1, &y1, &b);
            right = x1;
        } else {
            layout->HitTestTextPosition((UINT32)hi, FALSE, &x1, &y1, &b);
            right = x1;
        }
        if (right < left) {
            float tmp = left;
            left = right;
            right = tmp;
        }
        // A selection that carries on past this line covers the rest of its
        // line box, the way a text editor draws it.
        if (wb > visEnd && lo < visEnd) {
            right = tm.layoutWidth;
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
