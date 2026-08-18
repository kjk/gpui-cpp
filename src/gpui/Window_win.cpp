/* The Win32 window: message loop, chrome hit-testing, timers, clipboard, and
   the process entry point. Everything it decides is delegated to
   WindowCommon.cpp. */

#include "gpui/Platform.h"
#include "gpui/Paint.h"

#include <dwmapi.h>
#include <ole2.h>
#include <shellapi.h>

namespace gpui {

static const wchar_t* kWndClass = L"Gpui2SystemMonitor";

struct PlatWindow {
    HWND hwnd = nullptr;
    // WM_SETCURSOR fires on every move over the client area and has to put it
    // back, so the window remembers what it last asked for.
    HCURSOR cursor = nullptr;
};

static HWND Hwnd(Window* win) {
    return (win && win->plat) ? win->plat->hwnd : nullptr;
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

// user32!GetDpiForWindow is Windows 10 1607 and later, so it has to be
// resolved at runtime. HostDpi runs on every mouse move, so resolve it once
// and keep the answer.
typedef UINT(WINAPI* GetDpiForWindowFn)(HWND);

// The "looked it up and this Windows does not have it" sentinel. An address
// of 1 is not a possible GetProcAddress result, so one pointer carries all
// three states: null is not looked up yet, kNoDpiFn is missing, anything
// else is the function.
static GetDpiForWindowFn kNoDpiFn = (GetDpiForWindowFn)1;

static float HostDpi(HWND hwnd) {
    static GetDpiForWindowFn getDpiForWindow = nullptr;
    if (!getDpiForWindow) {
        HMODULE user = GetModuleHandleW(L"user32.dll");
        if (user) {
            getDpiForWindow =
                (GetDpiForWindowFn)GetProcAddress(user, "GetDpiForWindow");
        }
        if (!getDpiForWindow) {
            getDpiForWindow = kNoDpiFn;
        }
    }
    UINT dpi = getDpiForWindow != kNoDpiFn ? getDpiForWindow(hwnd) : 96;
    if (dpi == 0) {
        dpi = 96;
    }
    return (float)dpi;
}

static void RenderFrame(Window* win, HDC hdc) {
    HWND hwnd = Hwnd(win);
    if (!hwnd) {
        return;
    }
    RECT rc = {};
    GetClientRect(hwnd, &rc);
    // The DC render target is created at 96 dpi, so a DIP is a pixel.
    win->paint.dpi = 96;
    WINDOWPLACEMENT wp = {sizeof(wp)};
    GetWindowPlacement(hwnd, &wp);
    win->maximized = wp.showCmd == SW_SHOWMAXIMIZED;
    int pxW = rc.right - rc.left;
    int pxH = rc.bottom - rc.top;
    WindowDrawFrame(win, hdc, pxW, pxH, (float)pxW, (float)pxH);
}

static int BorderPx() {
    return GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
}

static bool ShiftDown() {
    return (GetKeyState(VK_SHIFT) & 0x8000) != 0;
}
static bool CtrlDown() {
    return (GetKeyState(VK_CONTROL) & 0x8000) != 0;
}
static bool AltDown() {
    return (GetKeyState(VK_MENU) & 0x8000) != 0;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                LPARAM lParam) {
    Window* win = (Window*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (msg == WM_NCCREATE) {
        auto* cs = (CREATESTRUCTW*)lParam;
        win = (Window*)cs->lpCreateParams;
        win->plat->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)win);
    }
    if (!win) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
        case WM_CREATE: {
            PlatSetTimer(win, WindowTimerMs(win));
            return 0;
        }
        case WM_KEYDOWN:
            WindowKeyDown(win, (int)wParam, ShiftDown(), CtrlDown(), AltDown());
            return 0;
        case WM_CHAR:
            WindowChar(win, (uint32_t)wParam, CtrlDown(), AltDown());
            return 0;
        case WM_RBUTTONDOWN: {
            float x = PxToDip(&win->paint, GET_X_LPARAM(lParam));
            float y = PxToDip(&win->paint, GET_Y_LPARAM(lParam));
            WindowMouseDown(win, x, y, 2);
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
            if (pt.y < BorderPx()) {
                return HTTOP;
            }
            float dipX = PxToDip(&win->paint, pt.x);
            float dipY = PxToDip(&win->paint, pt.y);
            switch (WindowChromeHit(win, dipX, dipY)) {
                case ClickWinMin:
                    return HTMINBUTTON;
                case ClickWinMax:
                    return HTMAXBUTTON;
                case ClickWinClose:
                    return HTCLOSE;
                case ClickWinCaption:
                    return HTCAPTION;
                default:
                    return HTCLIENT;
            }
        }
        case WM_SIZE:
            win->paint.dpi = HostDpi(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_DPICHANGED: {
            auto* r = (RECT*)lParam;
            SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left,
                         r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
            PaintTargetFree(&win->paint);
            return 0;
        }
        case WM_TIMER:
            WindowTimerTick(win);
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
            float x = PxToDip(&win->paint, GET_X_LPARAM(lParam));
            float y = PxToDip(&win->paint, GET_Y_LPARAM(lParam));
            WindowMouseMove(win, x, y);
            TRACKMOUSEEVENT tme = {sizeof(tme)};
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
            return 0;
        }
        case WM_MOUSELEAVE:
            WindowMouseLeave(win);
            return 0;
        case WM_LBUTTONDOWN: {
            win->paint.dpi = HostDpi(hwnd);
            float x = PxToDip(&win->paint, GET_X_LPARAM(lParam));
            float y = PxToDip(&win->paint, GET_Y_LPARAM(lParam));
            WindowMouseDown(win, x, y, 1);
            return 0;
        }
        case WM_LBUTTONUP: {
            float x = PxToDip(&win->paint, GET_X_LPARAM(lParam));
            float y = PxToDip(&win->paint, GET_Y_LPARAM(lParam));
            WindowMouseUp(win, x, y, 1);
            return 0;
        }
        case WM_LBUTTONDBLCLK: {
            float x = PxToDip(&win->paint, GET_X_LPARAM(lParam));
            float y = PxToDip(&win->paint, GET_Y_LPARAM(lParam));
            WindowDoubleClick(win, x, y);
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
            WindowWheel(win, x, y, delta);
            return 0;
        }
        case WM_SETCURSOR:
            // Only the client area; the frame's resize arrows are the
            // default handler's business.
            if (LOWORD(lParam) == HTCLIENT) {
                SetCursor(win->plat->cursor ? win->plat->cursor
                                            : LoadCursorW(nullptr, IDC_ARROW));
                return TRUE;
            }
            break;
        case WM_ERASEBKGND:
            return 1;
        case WM_DESTROY: {
            KillTimer(hwnd, 1);
            App* app = win->app;
            delete win->plat;
            WindowClosed(win);
            // The message loop ends when the last window closes.
            if (!AppAnyWindowOpen(app)) {
                PostQuitMessage(0);
            }
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ─── window commands ──────────────────────────────────────────────────────

void AppQuit(Window* win) {
    HWND hwnd = Hwnd(win);
    if (hwnd) {
        DestroyWindow(hwnd);
    }
}

void AppInvalidate(Window* win) {
    HWND hwnd = Hwnd(win);
    if (hwnd) {
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

void AppMinimize(Window* win) {
    HWND hwnd = Hwnd(win);
    if (hwnd) {
        ShowWindow(hwnd, SW_MINIMIZE);
    }
}

void AppToggleMaximize(Window* win) {
    HWND hwnd = Hwnd(win);
    if (!hwnd) {
        return;
    }
    WINDOWPLACEMENT wp = {sizeof(wp)};
    GetWindowPlacement(hwnd, &wp);
    ShowWindow(hwnd, wp.showCmd == SW_SHOWMAXIMIZED ? SW_RESTORE : SW_MAXIMIZE);
}

void AppDrag(Window* win) {
    HWND hwnd = Hwnd(win);
    if (hwnd) {
        ReleaseCapture();
        SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    }
}

void AppSetTitle(Window* win, Str title) {
    HWND hwnd = Hwnd(win);
    if (hwnd) {
        SetWindowTextW(hwnd, ToCWstrTemp(title));
    }
}

void PlatSetTimer(Window* win, int ms) {
    HWND hwnd = Hwnd(win);
    if (!hwnd) {
        return;
    }
    if (ms > 0) {
        SetTimer(hwnd, 1, (UINT)ms, nullptr);
    } else {
        KillTimer(hwnd, 1);
    }
}

void PlatSetCursor(Window* win, CursorKind kind) {
    if (!win || !win->plat) {
        return;
    }
    LPCWSTR name = kind == CursorKind::IBeam ? IDC_IBEAM : IDC_ARROW;
    win->plat->cursor = LoadCursorW(nullptr, name);
    SetCursor(win->plat->cursor);
}

void ClipboardSetText(Window* win, Str text) {
    HWND hwnd = Hwnd(win);
    if (!text.s || text.len <= 0) {
        return;
    }
    WCHAR* w = ToCWstrTemp(text);
    int wn = (int)wcslen(w);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)(wn + 1) * sizeof(WCHAR));
    if (!h) {
        return;
    }
    auto* dst = (WCHAR*)GlobalLock(h);
    if (!dst) {
        GlobalFree(h);
        return;
    }
    memcpy(dst, w, (size_t)(wn + 1) * sizeof(WCHAR));
    GlobalUnlock(h);
    if (!OpenClipboard(hwnd)) {
        GlobalFree(h);
        return;
    }
    EmptyClipboard();
    // The clipboard owns the handle from here on.
    SetClipboardData(CF_UNICODETEXT, h);
    CloseClipboard();
}

// ─── app lifecycle ────────────────────────────────────────────────────────

bool PlatInit(App* app) {
    (void)app;
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

    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = {sizeof(wc)};
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        // Null, so the default handler does not reset the pointer to an
        // arrow on every move; WM_SETCURSOR above owns it.
        wc.hCursor = nullptr;
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.lpszClassName = kWndClass;
        RegisterClassExW(&wc);
        registered = true;
    }
    return true;
}

void PlatShutdown(App* app) {
    (void)app;
    CoUninitialize();
}

Window* WindowOpen(App* app, Str title, int dipW, int dipH, WinOpts opts) {
    Window* win = WindowAlloc(app, opts);
    if (!win) {
        return nullptr;
    }
    win->plat = new PlatWindow();

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
        delete win->plat;
        win->plat = nullptr;
        return nullptr;
    }

    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark,
                          sizeof(dark));
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
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

} // namespace gpui

// The process entry point. Examples implement GpuiMain(argc, argv) and never
// see wWinMain or a UTF-16 command line.
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argc = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!wargv || argc <= 0) {
        char* argv0 = (char*)"gpui";
        char* argv[2] = {argv0, nullptr};
        return GpuiMain(1, argv);
    }
    // One UTF-8 block for the strings plus the pointer array; both live for
    // the whole process, so nothing frees them.
    auto** argv = (char**)calloc((size_t)argc + 1, sizeof(char*));
    if (!argv) {
        return 1;
    }
    for (int i = 0; i < argc; i++) {
        int n = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, nullptr, 0,
                                    nullptr, nullptr);
        if (n <= 0) {
            n = 1;
        }
        auto* buf = (char*)calloc((size_t)n, 1);
        if (!buf) {
            return 1;
        }
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, buf, n, nullptr, nullptr);
        argv[i] = buf;
    }
    LocalFree(wargv);
    return GpuiMain(argc, argv);
}
