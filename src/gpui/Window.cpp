#include "gpui/Gpui.h"

#include <ole2.h>

namespace gpui {

static const wchar_t* kWndClass = L"Gpui2SystemMonitor";
static const UINT kTickMs = 500;

static void SafeRelease_(IUnknown** p) {
    if (p && *p) {
        (*p)->Release();
        *p = nullptr;
    }
}

template <typename T>
static void Rel(T** p) {
    SafeRelease_((IUnknown**)p);
}

double TimeNow() {
    static LARGE_INTEGER freq = {};
    static LARGE_INTEGER start = {};
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&start);
    }
    LARGE_INTEGER now = {};
    QueryPerformanceCounter(&now);
    return (double)(now.QuadPart - start.QuadPart) / (double)freq.QuadPart;
}

int WindowCollectFrames(Window* win, uint64_t* cursor, FrameTiming* out,
                        int max) {
    if (!win || !cursor || !out || max <= 0) {
        return 0;
    }
    uint64_t from = *cursor;
    // Frames that fell out of the ring while nobody was collecting are gone.
    if (win->frameSeq > (uint64_t)kFrameTraceCap &&
        from < win->frameSeq - (uint64_t)kFrameTraceCap) {
        from = win->frameSeq - (uint64_t)kFrameTraceCap;
    }
    if (from + (uint64_t)max < win->frameSeq) {
        from = win->frameSeq - (uint64_t)max;
    }
    int n = 0;
    for (uint64_t i = from; i < win->frameSeq; i++) {
        out[n++] = win->frameTrace[i % (uint64_t)kFrameTraceCap];
    }
    *cursor = win->frameSeq;
    return n;
}

static float HostDpi(HWND hwnd) {
    UINT dpi = 96;
    HMODULE user = GetModuleHandleW(L"user32.dll");
    if (user) {
        typedef UINT(WINAPI * GetDpiForWindowFn)(HWND);
        auto fn = (GetDpiForWindowFn)GetProcAddress(user, "GetDpiForWindow");
        if (fn) {
            dpi = fn(hwnd);
        }
    }
    if (dpi == 0) {
        dpi = 96;
    }
    return (float)dpi;
}

static void UpdateDipSize(Window* win) {
    RECT rc = {};
    GetClientRect(win->hwnd, &rc);
    win->paint.dpi = HostDpi(win->hwnd);
    int pxW = rc.right - rc.left;
    int pxH = rc.bottom - rc.top;
    // unused but kept for future pixel-accurate work
    (void)pxW;
    (void)pxH;
}

static HRESULT CreateDeviceResources(Window* win) {
    if (win->paint.dcRt) {
        win->paint.rt = win->paint.dcRt;
        return S_OK;
    }
    win->paint.dpi = 96;
    D2D1_RENDER_TARGET_PROPERTIES rtp = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
        96.f, 96.f);
    HRESULT hr = win->paint.d2d->CreateDCRenderTarget(&rtp, &win->paint.dcRt);
    if (FAILED(hr)) {
        logf("CreateDCRenderTarget failed %08x", (unsigned)hr);
        return hr;
    }
    win->paint.rt = win->paint.dcRt;
    hr = win->paint.rt
             ->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1), &win->paint.brush);
    return hr;
}

static void DiscardDeviceResources(Window* win) {
    Rel(&win->paint.brush);
    Rel(&win->paint.dcRt);
    win->paint.rt = nullptr;
}

static void RenderFrame(Window* win, HDC hdc) {
    if (FAILED(CreateDeviceResources(win))) {
        return;
    }
    double drawStart = TimeNow();
    RECT rc = {};
    GetClientRect(win->hwnd, &rc);
    HRESULT bindHr = win->paint.dcRt->BindDC(hdc, &rc);
    if (FAILED(bindHr)) {
        logf("BindDC failed %08x", (unsigned)bindHr);
        DiscardDeviceResources(win);
        return;
    }
    win->paint.dpi = 96;
    float dipW = (float)(rc.right - rc.left);
    float dipH = (float)(rc.bottom - rc.top);

    WINDOWPLACEMENT wp = {sizeof(wp)};
    GetWindowPlacement(win->hwnd, &wp);
    win->maximized = wp.showCmd == SW_SHOWMAXIMIZED;

    if (win->frameArena) {
        win->frameArena->Reset();
    } else {
        win->frameArena = ArenaNew();
    }
    ResetTempArena();
    win->paint.hits.Clear();
    win->paint.scrolls.Clear();
    win->paint.texts.Clear();
    win->paint.textDocLen = 0;
    win->paint.selA = -1;
    win->paint.selB = -1;
    win->paint.hoverId = win->hoverId;
    win->paint.focusId = win->focusId;
    win->paint.viewW = dipW;
    win->paint.viewH = dipH;
    TextMeasBeginFrame(&win->paint);

    WinSize ws;
    ws.dipW = dipW;
    ws.dipH = dipH;
    ws.pxW = rc.right - rc.left;
    ws.pxH = rc.bottom - rc.top;

    (void)ws;
    El* root = EntityRender(win->app, win, win->frameArena, win->root);

    win->paint.rt->BeginDraw();
    win->paint.rt->SetTransform(D2D1::Matrix3x2F::Identity());
    const Theme& th = ThemeNow();
    win->paint.rt->Clear(RgbaToD2D(th.background));

    if (root) {
        LayoutEl(&win->paint, root, 0, 0, dipW, dipH, 16.f, th.foreground);
        FocusCollect(win, root);
        PaintEl(&win->paint, root);
    }

    HRESULT hr = win->paint.rt->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources(win);
    }
    TextMeasEndFrame(&win->paint);

    // Record the frame for the trace. GPUI times Window::draw, which is this
    // whole function: build the element tree, lay it out, paint it.
    FrameTiming timing;
    timing.drawSecs = (float)(TimeNow() - drawStart);
    win->frameTrace[win->frameSeq % (uint64_t)kFrameTraceCap] = timing;
    win->frameSeq++;
}

static int BorderPx() {
    return GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                LPARAM lParam) {
    Window* win = (Window*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (msg == WM_NCCREATE) {
        auto* cs = (CREATESTRUCTW*)lParam;
        win = (Window*)cs->lpCreateParams;
        win->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)win);
    }
    if (!win) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
        case WM_CREATE: {
            if (win->opts.anim || win->tickMs > 0) {
                int ms = win->opts.timerMs > 0 ? win->opts.timerMs : kTickMs;
                if (win->tickMs > 0) {
                    ms = win->tickMs;
                }
                if (win->opts.anim) {
                    ms = 16;
                }
                SetTimer(hwnd, 1, (UINT)ms, nullptr);
            }
            return 0;
        }
        case WM_KEYDOWN: {
            if (wParam == VK_TAB) {
                bool back = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                int trap = 0;
                for (int i = 0; i < win->focusEls.len; i++) {
                    if (win->focusEls[i].id == win->focusId) {
                        trap = win->focusEls[i].trapId;
                        break;
                    }
                }
                FocusNext(win, trap, back);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (win->onKey.IsValid()) {
                KeyEvent ev = {(int)wParam, 0, true};
                ListenerCall(win->app, win, win->onKey, &ev);
            }
            // Enter activates the focused element: run that element's own
            // listener, the one a click on it would have run.
            if (wParam == VK_RETURN && win->focusId && !win->eatReturn) {
                const HitRect* focused = nullptr;
                for (int i = win->paint.hits.len - 1; i >= 0; i--) {
                    if (win->paint.hits[i].id == win->focusId) {
                        focused = &win->paint.hits[i];
                        break;
                    }
                }
                ClickEvent ev = {0, 0, 1, win->focusId};
                if (focused) {
                    ev.x = focused->x + focused->w * 0.5f;
                    ev.y = focused->y + focused->h * 0.5f;
                }
                if (focused && focused->listener.IsValid()) {
                    ListenerCall(win->app, win, focused->listener, &ev);
                } else if (win->onClick.IsValid()) {
                    ListenerCall(win->app, win, win->onClick, &ev);
                }
            }
            win->eatReturn = false;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_CHAR: {
            if (win->onKey.IsValid() && wParam >= 32) {
                KeyEvent ev = {0, (uint32_t)wParam, true};
                ListenerCall(win->app, win, win->onKey, &ev);
            }
            if (win->input && win->input->focused) {
                LineInput* in = win->input;
                if (wParam == 8) {
                    if (in->len > 0) {
                        in->len--;
                        in->buf[in->len] = 0;
                        in->cursor = in->len;
                    }
                } else if (wParam >= 32 && wParam < 127 && in->len < 511) {
                    in->buf[in->len++] = (char)wParam;
                    in->buf[in->len] = 0;
                    in->cursor = in->len;
                }
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_RBUTTONDOWN: {
            float x = PxToDip(&win->paint, GET_X_LPARAM(lParam));
            float y = PxToDip(&win->paint, GET_Y_LPARAM(lParam));
            if (win->onMouse.IsValid()) {
                MouseEvent ev = {MouseKind::Down, x, y, 2, 0};
                ListenerCall(win->app, win, win->onMouse, &ev);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_NCCALCSIZE:
            break;
        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProcW(hwnd, msg, wParam, lParam);
            if (hit != HTCLIENT) {
                return hit;
            }
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(hwnd, &pt);
            RECT rc = {};
            GetClientRect(hwnd, &rc);
            int b = BorderPx();
            if (pt.y < b) {
                return HTTOP;
            }
            float dipX = PxToDip(&win->paint, pt.x);
            float dipY = PxToDip(&win->paint, pt.y);
            int id = HitTest(&win->paint, dipX, dipY);
            if (id == ClickWinMin) {
                return HTMINBUTTON;
            }
            if (id == ClickWinMax) {
                return HTMAXBUTTON;
            }
            if (id == ClickWinClose) {
                return HTCLOSE;
            }
            if (id == ClickWinCaption) {
                return HTCAPTION;
            }
            return HTCLIENT;
        }
        case WM_SIZE:
            UpdateDipSize(win);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_DPICHANGED: {
            auto* r = (RECT*)lParam;
            SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left,
                         r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
            DiscardDeviceResources(win);
            return 0;
        }
        case WM_TIMER:
            if (win->onTick.IsValid()) {
                TickEvent ev = {win->tickMs};
                ListenerCall(win->app, win, win->onTick, &ev);
            }
            if (win->anim || win->onTick.IsValid()) {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RenderFrame(win, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_MOUSEMOVE: {
            win->paint.dpi = HostDpi(hwnd);
            win->mouseX = PxToDip(&win->paint, GET_X_LPARAM(lParam));
            win->mouseY = PxToDip(&win->paint, GET_Y_LPARAM(lParam));
            int id = HitTest(&win->paint, win->mouseX, win->mouseY);
            if (id != win->hoverId) {
                win->hoverId = id;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            if (win->onMouse.IsValid()) {
                MouseEvent ev = {MouseKind::Move, win->mouseX, win->mouseY, 0,
                                 id};
                ListenerCall(win->app, win, win->onMouse, &ev);
            }
            if (win->mouseDown) {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            TRACKMOUSEEVENT tme = {sizeof(tme)};
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
            return 0;
        }
        case WM_MOUSELEAVE:
            win->hoverId = 0;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_LBUTTONDOWN: {
            win->paint.dpi = HostDpi(hwnd);
            float x = PxToDip(&win->paint, GET_X_LPARAM(lParam));
            float y = PxToDip(&win->paint, GET_Y_LPARAM(lParam));
            const HitRect* hit = HitTestRect(&win->paint, x, y);
            int id = hit ? hit->id : 0;
            win->mouseDown = true;
            if (id) {
                win->focusId = id;
            }
            if (win->onMouse.IsValid()) {
                MouseEvent ev = {MouseKind::Down, x, y, 1, id};
                ListenerCall(win->app, win, win->onMouse, &ev);
            }
            if (hit && hit->listener.IsValid()) {
                ClickEvent ev = {x, y, 1, id};
                ListenerCall(win->app, win, hit->listener, &ev);
            } else if (win->onClick.IsValid()) {
                ClickEvent ev = {x, y, 1, id};
                ListenerCall(win->app, win, win->onClick, &ev);
            }
            if (hit && hit->onClick.IsValid()) {
                hit->onClick.Call();
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONUP: {
            float x = PxToDip(&win->paint, GET_X_LPARAM(lParam));
            float y = PxToDip(&win->paint, GET_Y_LPARAM(lParam));
            win->mouseDown = false;
            if (win->onMouse.IsValid()) {
                MouseEvent ev = {MouseKind::Up, x, y, 1, 0};
                ListenerCall(win->app, win, win->onMouse, &ev);
            }
            return 0;
        }
        case WM_LBUTTONDBLCLK: {
            float x = PxToDip(&win->paint, GET_X_LPARAM(lParam));
            float y = PxToDip(&win->paint, GET_Y_LPARAM(lParam));
            int id = HitTest(&win->paint, x, y);
            if (id == ClickWinCaption || (id == 0 && y < 34)) {
                AppToggleMaximize(win);
            }
            return 0;
        }
        case WM_NCLBUTTONDOWN:
            if (wParam == HTMINBUTTON) {
                AppMinimize(win);
                return 0;
            }
            if (wParam == HTMAXBUTTON) {
                AppToggleMaximize(win);
                return 0;
            }
            if (wParam == HTCLOSE) {
                AppClose(win);
                return 0;
            }
            break;
        case WM_MOUSEWHEEL: {
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(hwnd, &pt);
            float x = PxToDip(&win->paint, pt.x);
            float y = PxToDip(&win->paint, pt.y);
            float delta = (float)GET_WHEEL_DELTA_WPARAM(wParam) /
                          (float)WHEEL_DELTA * 48.f;
            if (win->onWheel.IsValid()) {
                WheelEvent ev = {x, y, delta};
                ListenerCall(win->app, win, win->onWheel, &ev);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_DESTROY: {
            KillTimer(hwnd, 1);
            DiscardDeviceResources(win);
            win->hwnd = nullptr;
            win->running = false;
            // The message loop ends when the last window closes.
            App* app = win->app;
            bool anyLeft = false;
            if (app) {
                for (int i = 0; i < app->windows.len; i++) {
                    if (app->windows[i]->hwnd) {
                        anyLeft = true;
                        break;
                    }
                }
            }
            if (!anyLeft) {
                PostQuitMessage(0);
            }
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void AppQuit(Window* win) {
    if (win && win->hwnd) {
        DestroyWindow(win->hwnd);
    }
}
void AppInvalidate(Window* win) {
    if (win && win->hwnd) {
        InvalidateRect(win->hwnd, nullptr, FALSE);
    }
}
void AppMinimize(Window* win) {
    if (win && win->hwnd) {
        ShowWindow(win->hwnd, SW_MINIMIZE);
    }
}
void AppToggleMaximize(Window* win) {
    if (!win || !win->hwnd) {
        return;
    }
    WINDOWPLACEMENT wp = {sizeof(wp)};
    GetWindowPlacement(win->hwnd, &wp);
    ShowWindow(win->hwnd,
               wp.showCmd == SW_SHOWMAXIMIZED ? SW_RESTORE : SW_MAXIMIZE);
}
void AppClose(Window* win) {
    AppQuit(win);
}
void AppDrag(Window* win) {
    if (win && win->hwnd) {
        ReleaseCapture();
        SendMessageW(win->hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    }
}
bool AppIsMaximized(Window* win) {
    return win && win->maximized;
}

void AppSetTitle(Window* win, Str title) {
    if (win && win->hwnd) {
        SetWindowTextW(win->hwnd, ToCWstrTemp(title));
    }
}

void AppRequestAnim(Window* win, bool on) {
    if (win) {
        win->anim = on;
        win->opts.anim = on;
        if (win->hwnd) {
            if (on) {
                SetTimer(win->hwnd, 1, 16u, nullptr);
            } else if (win->tickMs > 0) {
                UINT ms =
                    win->opts.timerMs > 0 ? (UINT)win->opts.timerMs : kTickMs;
                SetTimer(win->hwnd, 1, ms, nullptr);
            } else {
                KillTimer(win->hwnd, 1);
            }
        }
    }
}

// ─── app lifecycle ────────────────────────────────────────────────────────

static bool gClassRegistered = false;

static void RegisterWndClass() {
    if (gClassRegistered) {
        return;
    }
    WNDCLASSEXW wc = {sizeof(wc)};
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = kWndClass;
    RegisterClassExW(&wc);
    gClassRegistered = true;
}

static void MakeFontFamily(App* app, const wchar_t* family, float px,
                           int weight, bool wrap, IDWriteTextFormat** out) {
    DWRITE_FONT_WEIGHT w =
        weight ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL;
    app->dwrite
        ->CreateTextFormat(family, nullptr, w, DWRITE_FONT_STYLE_NORMAL,
                           DWRITE_FONT_STRETCH_NORMAL, px, L"en-us", out);
    if (*out && !wrap) {
        (*out)->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }
}

static void MakeFont(App* app, float px, int weight, bool wrap,
                     IDWriteTextFormat** out) {
    MakeFontFamily(app, L"Segoe UI", px, weight, wrap, out);
}

App* AppNew() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        typedef BOOL(WINAPI * SetDpiFn)(HANDLE);
        auto setDpi =
            (SetDpiFn)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        if (setDpi) {
            setDpi((HANDLE)-4); // PER_MONITOR_AWARE_V2
        }
    }

    App* app = new App();
    HRESULT hr =
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &app->d2d);
    if (FAILED(hr)) {
        delete app;
        return nullptr;
    }
    hr =
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                            __uuidof(IDWriteFactory), (IUnknown**)&app->dwrite);
    if (FAILED(hr)) {
        Rel(&app->d2d);
        delete app;
        return nullptr;
    }
    MakeFont(app, 16.f, 0, false, &app->font16);
    MakeFont(app, 14.f, 0, false, &app->font14);
    MakeFont(app, 12.f, 0, false, &app->font12);
    MakeFont(app, 20.f, 1, false, &app->font20);
    MakeFont(app, 24.f, 1, true, &app->font24);
    MakeFont(app, 16.f, 1, true, &app->font16b);
    // The monospace family the FPS HUD asks for. Consolas ships with Windows,
    // so nothing has to configure a font for its columns to line up.
    MakeFontFamily(app, L"Consolas", 12.f, 0, false, &app->fontMono);
    if (app->font24) {
        app->font24->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }
    RegisterWndClass();
    return app;
}

void AppFree(App* app) {
    if (!app) {
        return;
    }
    EntityDropAll(app);
    for (int i = 0; i < app->windows.len; i++) {
        Window* w = app->windows[i];
        if (w->frameArena) {
            ArenaDelete(w->frameArena);
        }
        TextMeasClear(&w->paint);
        WindowKeyedFree(w);
        delete w;
    }
    app->windows.Reset();
    Rel(&app->font12);
    Rel(&app->font14);
    Rel(&app->font16);
    Rel(&app->font20);
    Rel(&app->font24);
    Rel(&app->font16b);
    Rel(&app->fontMono);
    Rel(&app->dwrite);
    Rel(&app->d2d);
    delete app;
    DestroyTempArena();
    CoUninitialize();
}

Window* WindowOpen(App* app, Str title, int dipW, int dipH, WinOpts opts) {
    if (!app) {
        return nullptr;
    }
    Window* win = new Window();
    win->app = app;
    win->opts = opts;
    win->anim = opts.anim;
    // The factories and the font cache live on App; each window borrows them.
    win->paint.d2d = app->d2d;
    win->paint.dwrite = app->dwrite;
    win->paint.font16 = app->font16;
    win->paint.font14 = app->font14;
    win->paint.font12 = app->font12;
    win->paint.font20 = app->font20;
    win->paint.font24 = app->font24;
    win->paint.font16b = app->font16b;
    win->paint.fontMono = app->fontMono;
    app->windows.Append(win);

    DWORD style = WS_OVERLAPPEDWINDOW;
    if (opts.borderless) {
        style = WS_OVERLAPPEDWINDOW & ~WS_CAPTION;
        style |= WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    }
    RECT wr = {0, 0, dipW, dipH};
    AdjustWindowRectEx(&wr, style, FALSE, 0);
    int pxW = wr.right - wr.left;
    int pxH = wr.bottom - wr.top;
    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);
    int x = (sx - pxW) / 2;
    int y = (sy - pxH) / 2;

    HWND hwnd =
        CreateWindowExW(0, kWndClass, ToCWstrTemp(title), style, x, y, pxW, pxH,
                        nullptr, nullptr, GetModuleHandleW(nullptr), win);
    if (!hwnd) {
        return nullptr;
    }

    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark,
                          sizeof(dark));
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    return win;
}

Window* WindowOpenView(App* app, Str title, int dipW, int dipH, EntityId root,
                       WinOpts opts) {
    Window* win = WindowOpen(app, title, dipW, dipH, opts);
    if (win) {
        win->root = root;
        AppInvalidate(win);
    }
    return win;
}

int AppRun(App* app) {
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (app) {
        app->exitCode = (int)msg.wParam;
    }
    return (int)msg.wParam;
}

int AppRunView(Str title, int dipW, int dipH, EntityId root, App* app,
               WinOpts opts) {
    if (!WindowOpenView(app, title, dipW, dipH, root, opts)) {
        return 1;
    }
    int rc = AppRun(app);
    AppFree(app);
    return rc;
}

} // namespace gpui
