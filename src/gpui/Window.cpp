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

static void UpdateDipSize(Window* host) {
    RECT rc = {};
    GetClientRect(host->hwnd, &rc);
    host->paint.dpi = HostDpi(host->hwnd);
    int pxW = rc.right - rc.left;
    int pxH = rc.bottom - rc.top;
    // unused but kept for future pixel-accurate work
    (void)pxW;
    (void)pxH;
}

static HRESULT CreateDeviceResources(Window* host) {
    if (host->paint.dcRt) {
        host->paint.rt = host->paint.dcRt;
        return S_OK;
    }
    host->paint.dpi = 96;
    D2D1_RENDER_TARGET_PROPERTIES rtp = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
        96.f, 96.f);
    HRESULT hr = host->paint.d2d->CreateDCRenderTarget(&rtp, &host->paint.dcRt);
    if (FAILED(hr)) {
        logf("CreateDCRenderTarget failed %08x", (unsigned)hr);
        return hr;
    }
    host->paint.rt = host->paint.dcRt;
    hr = host->paint.rt
             ->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1), &host->paint.brush);
    return hr;
}

static void DiscardDeviceResources(Window* host) {
    Rel(&host->paint.brush);
    Rel(&host->paint.dcRt);
    host->paint.rt = nullptr;
}

static void RenderFrame(Window* host, HDC hdc) {
    if (FAILED(CreateDeviceResources(host))) {
        return;
    }
    RECT rc = {};
    GetClientRect(host->hwnd, &rc);
    HRESULT bindHr = host->paint.dcRt->BindDC(hdc, &rc);
    if (FAILED(bindHr)) {
        logf("BindDC failed %08x", (unsigned)bindHr);
        DiscardDeviceResources(host);
        return;
    }
    host->paint.dpi = 96;
    float dipW = (float)(rc.right - rc.left);
    float dipH = (float)(rc.bottom - rc.top);

    WINDOWPLACEMENT wp = {sizeof(wp)};
    GetWindowPlacement(host->hwnd, &wp);
    host->maximized = wp.showCmd == SW_SHOWMAXIMIZED;

    if (host->frameArena) {
        host->frameArena->Reset();
    } else {
        host->frameArena = ArenaNew();
    }
    ResetTempArena();
    host->paint.hits.Clear();
    host->paint.scrolls.Clear();
    host->paint.texts.Clear();
    host->paint.textDocLen = 0;
    host->paint.selA = -1;
    host->paint.selB = -1;
    host->paint.hoverId = host->hoverId;
    host->paint.focusId = host->focusId;
    host->paint.viewW = dipW;
    host->paint.viewH = dipH;
    TextMeasBeginFrame(&host->paint);

    WinSize ws;
    ws.dipW = dipW;
    ws.dipH = dipH;
    ws.pxW = rc.right - rc.left;
    ws.pxH = rc.bottom - rc.top;

    El* root = nullptr;
    if (host->root.IsValid()) {
        root = EntityRender(host->app, host, host->frameArena, host->root);
    } else if (host->hooks.onRender) {
        root = host->hooks.onRender(host, host->frameArena, ws);
    }

    host->paint.rt->BeginDraw();
    host->paint.rt->SetTransform(D2D1::Matrix3x2F::Identity());
    const Theme& th = ThemeNow();
    host->paint.rt->Clear(RgbaToD2D(th.background));

    if (root) {
        LayoutEl(&host->paint, root, 0, 0, dipW, dipH, 16.f, th.foreground);
        FocusCollect(host, root);
        PaintEl(&host->paint, root);
    }

    HRESULT hr = host->paint.rt->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources(host);
    }
    TextMeasEndFrame(&host->paint);
}

static int BorderPx() {
    return GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                LPARAM lParam) {
    Window* host = (Window*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (msg == WM_NCCREATE) {
        auto* cs = (CREATESTRUCTW*)lParam;
        host = (Window*)cs->lpCreateParams;
        host->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)host);
    }
    if (!host) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
        case WM_CREATE: {
            if (host->winOpts.anim || host->hooks.onTick) {
                int ms =
                    host->winOpts.timerMs > 0 ? host->winOpts.timerMs : kTickMs;
                if (host->winOpts.anim) {
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
                for (int i = 0; i < host->focusEls.len; i++) {
                    if (host->focusEls[i].id == host->focusId) {
                        trap = host->focusEls[i].trapId;
                        break;
                    }
                }
                FocusNext(host, trap, back);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (host->hooks.onKey) {
                host->hooks.onKey(host, (int)wParam, true);
            }
            if (wParam == VK_RETURN && host->focusId && host->hooks.onClick &&
                !host->eatReturn) {
                host->hooks.onClick(host, host->focusId);
            }
            host->eatReturn = false;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_CHAR: {
            if (host->hooks.onChar && wParam >= 32) {
                host->hooks.onChar(host, (uint32_t)wParam);
            }
            if (host->input && host->input->focused) {
                LineInput* in = host->input;
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
            float x = PxToDip(&host->paint, GET_X_LPARAM(lParam));
            float y = PxToDip(&host->paint, GET_Y_LPARAM(lParam));
            if (host->hooks.onMouseDown) {
                host->hooks.onMouseDown(host, x, y, 2);
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
            float dipX = PxToDip(&host->paint, pt.x);
            float dipY = PxToDip(&host->paint, pt.y);
            int id = HitTest(&host->paint, dipX, dipY);
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
            UpdateDipSize(host);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_DPICHANGED: {
            auto* r = (RECT*)lParam;
            SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left,
                         r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
            DiscardDeviceResources(host);
            return 0;
        }
        case WM_TIMER:
            if (host->hooks.onTick) {
                host->hooks.onTick(host);
            }
            if (host->anim || host->hooks.onTick) {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RenderFrame(host, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_MOUSEMOVE: {
            host->paint.dpi = HostDpi(hwnd);
            host->mouseX = PxToDip(&host->paint, GET_X_LPARAM(lParam));
            host->mouseY = PxToDip(&host->paint, GET_Y_LPARAM(lParam));
            int id = HitTest(&host->paint, host->mouseX, host->mouseY);
            if (id != host->hoverId) {
                host->hoverId = id;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            if (host->hooks.onMouseMove) {
                host->hooks.onMouseMove(host, host->mouseX, host->mouseY);
            }
            if (host->mouseDown) {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            TRACKMOUSEEVENT tme = {sizeof(tme)};
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
            return 0;
        }
        case WM_MOUSELEAVE:
            host->hoverId = 0;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_LBUTTONDOWN: {
            host->paint.dpi = HostDpi(hwnd);
            float x = PxToDip(&host->paint, GET_X_LPARAM(lParam));
            float y = PxToDip(&host->paint, GET_Y_LPARAM(lParam));
            const HitRect* hit = HitTestRect(&host->paint, x, y);
            int id = hit ? hit->id : 0;
            host->mouseDown = true;
            if (id) {
                host->focusId = id;
            }
            if (host->hooks.onMouseDown) {
                host->hooks.onMouseDown(host, x, y, 1);
            }
            if (hit && hit->listener.IsValid()) {
                ClickEvent ev = {x, y, 1};
                ListenerCall(host->app, host, hit->listener, &ev);
            }
            if (hit && hit->onClick.IsValid()) {
                hit->onClick.Call();
            }
            if (host->hooks.onClick) {
                host->hooks.onClick(host, id);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONUP: {
            float x = PxToDip(&host->paint, GET_X_LPARAM(lParam));
            float y = PxToDip(&host->paint, GET_Y_LPARAM(lParam));
            host->mouseDown = false;
            if (host->hooks.onMouseUp) {
                host->hooks.onMouseUp(host, x, y, 1);
            }
            return 0;
        }
        case WM_LBUTTONDBLCLK: {
            float x = PxToDip(&host->paint, GET_X_LPARAM(lParam));
            float y = PxToDip(&host->paint, GET_Y_LPARAM(lParam));
            int id = HitTest(&host->paint, x, y);
            if (id == ClickWinCaption || (id == 0 && y < 34)) {
                AppToggleMaximize(host);
            }
            return 0;
        }
        case WM_NCLBUTTONDOWN:
            if (wParam == HTMINBUTTON) {
                AppMinimize(host);
                return 0;
            }
            if (wParam == HTMAXBUTTON) {
                AppToggleMaximize(host);
                return 0;
            }
            if (wParam == HTCLOSE) {
                AppClose(host);
                return 0;
            }
            break;
        case WM_MOUSEWHEEL: {
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(hwnd, &pt);
            float x = PxToDip(&host->paint, pt.x);
            float y = PxToDip(&host->paint, pt.y);
            float delta = (float)GET_WHEEL_DELTA_WPARAM(wParam) /
                          (float)WHEEL_DELTA * 48.f;
            if (host->hooks.onWheel) {
                host->hooks.onWheel(host, x, y, delta);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_DESTROY: {
            KillTimer(hwnd, 1);
            if (host->hooks.onShutdown) {
                host->hooks.onShutdown(host);
            }
            DiscardDeviceResources(host);
            host->hwnd = nullptr;
            host->running = false;
            // The message loop ends when the last window closes.
            App* app = host->app;
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

void AppQuit(Window* host) {
    if (host && host->hwnd) {
        DestroyWindow(host->hwnd);
    }
}
void AppInvalidate(Window* host) {
    if (host && host->hwnd) {
        InvalidateRect(host->hwnd, nullptr, FALSE);
    }
}
void AppMinimize(Window* host) {
    if (host && host->hwnd) {
        ShowWindow(host->hwnd, SW_MINIMIZE);
    }
}
void AppToggleMaximize(Window* host) {
    if (!host || !host->hwnd) {
        return;
    }
    WINDOWPLACEMENT wp = {sizeof(wp)};
    GetWindowPlacement(host->hwnd, &wp);
    ShowWindow(host->hwnd,
               wp.showCmd == SW_SHOWMAXIMIZED ? SW_RESTORE : SW_MAXIMIZE);
}
void AppClose(Window* host) {
    AppQuit(host);
}
void AppDrag(Window* host) {
    if (host && host->hwnd) {
        ReleaseCapture();
        SendMessageW(host->hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    }
}
bool AppIsMaximized(Window* host) {
    return host && host->maximized;
}

void AppSetTitle(Window* host, const wchar_t* title) {
    if (host && host->hwnd && title) {
        SetWindowTextW(host->hwnd, title);
    }
}

void AppRequestAnim(Window* host, bool on) {
    if (host) {
        host->anim = on;
        host->winOpts.anim = on;
        if (host->hwnd) {
            if (on) {
                SetTimer(host->hwnd, 1, 16u, nullptr);
            } else if (host->hooks.onTick) {
                UINT ms = host->winOpts.timerMs > 0
                              ? (UINT)host->winOpts.timerMs
                              : kTickMs;
                SetTimer(host->hwnd, 1, ms, nullptr);
            } else {
                KillTimer(host->hwnd, 1);
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

static void MakeFont(App* app, float px, int weight, bool wrap,
                     IDWriteTextFormat** out) {
    DWRITE_FONT_WEIGHT w =
        weight ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL;
    app->dwrite
        ->CreateTextFormat(L"Segoe UI", nullptr, w, DWRITE_FONT_STYLE_NORMAL,
                           DWRITE_FONT_STRETCH_NORMAL, px, L"en-us", out);
    if (*out && !wrap) {
        (*out)->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }
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
    Rel(&app->dwrite);
    Rel(&app->d2d);
    delete app;
    DestroyTempArena();
    CoUninitialize();
}

Window* WindowOpen(App* app, const wchar_t* title, int dipW, int dipH,
                   AppHooks hooks, void* user, AppWinOpts opts) {
    if (!app) {
        return nullptr;
    }
    Window* win = new Window();
    win->app = app;
    win->hooks = hooks;
    win->user = user;
    win->winOpts = opts;
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
    app->windows.Append(win);

    if (win->hooks.onInit) {
        win->hooks.onInit(win);
    }

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
        CreateWindowExW(0, kWndClass, title, style, x, y, pxW, pxH, nullptr,
                        nullptr, GetModuleHandleW(nullptr), win);
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

Window* WindowOpenView(App* app, const wchar_t* title, int dipW, int dipH,
                       EntityId root, AppWinOpts opts) {
    AppHooks hooks = {};
    Window* win = WindowOpen(app, title, dipW, dipH, hooks, nullptr, opts);
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

int AppRunView(const wchar_t* title, int dipW, int dipH, EntityId root,
               App* app, AppWinOpts opts) {
    if (!WindowOpenView(app, title, dipW, dipH, root, opts)) {
        return 1;
    }
    int rc = AppRun(app);
    AppFree(app);
    return rc;
}

int RunApp(const wchar_t* title, int dipW, int dipH, AppHooks hooks,
           void* user) {
    AppWinOpts opts = {};
    return RunAppEx(title, dipW, dipH, hooks, user, opts);
}

int RunAppEx(const wchar_t* title, int dipW, int dipH, AppHooks hooks,
             void* user, AppWinOpts opts) {
    App* app = AppNew();
    if (!app) {
        return 1;
    }
    if (!WindowOpen(app, title, dipW, dipH, hooks, user, opts)) {
        AppFree(app);
        return 1;
    }
    int rc = AppRun(app);
    AppFree(app);
    return rc;
}

} // namespace gpui
