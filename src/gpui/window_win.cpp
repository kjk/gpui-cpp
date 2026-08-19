/* The Win32 window: message loop, chrome hit-testing, timers, clipboard, and
   the process entry point. Everything it decides is delegated to
   WindowCommon.cpp. */

#include "gpui/platform.h"
#include "gpui/paint.h"

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
    // WM_MOUSEACTIVATE said this next press is the one that activated the
    // window: MouseDownEvent::first_mouse.
    bool firstMouse = false;
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

static int BorderYPx() {
    return GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
}

// A window whose client area reaches the top edge because the view draws the
// title bar: WinOpts::clientTitleBar, and the older borderless flag that means
// the same thing here.
static bool ClientDecorated(Window* win) {
    return win->opts.clientTitleBar || win->opts.borderless;
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

// GPUI's Modifiers. Windows reports no Fn key, so `function` stays false.
static Modifiers ModsNow() {
    Modifiers m;
    m.control = CtrlDown();
    m.alt = AltDown();
    m.shift = ShiftDown();
    m.platform = (GetKeyState(VK_LWIN) & 0x8000) != 0 ||
                 (GetKeyState(VK_RWIN) & 0x8000) != 0;
    return m;
}

// Rust's Option<MouseButton> on a move: the first button that is down, or
// none. The MK_* bits in wParam say the same thing, but only for the messages
// that carry them -- WM_NCMOUSEMOVE's wParam is a hit-test code -- so this
// asks the keyboard state, which every path can.
static bool PressedButton(MouseButton* out) {
    struct {
        int vk;
        MouseButton button;
    } kButtons[] = {
        {VK_LBUTTON, MouseButton::Left},
        {VK_RBUTTON, MouseButton::Right},
        {VK_MBUTTON, MouseButton::Middle},
        {VK_XBUTTON1, MouseButton::NavigateBack},
        {VK_XBUTTON2, MouseButton::NavigateForward},
    };
    for (const auto& b : kButtons) {
        if (GetKeyState(b.vk) & 0x8000) {
            *out = b.button;
            return true;
        }
    }
    return false;
}

// One press, whichever button it came from. WM_LBUTTONDBLCLK arrives here too:
// the class has CS_DBLCLKS, so that message replaces the second WM_LBUTTONDOWN
// of a run. It is still a press and still has to reach the element under it --
// Win32 only renamed the message. WindowClickCount is what numbers it, and
// what numbers the third press, which Win32 has no message for at all.
static void MouseDown(Window* win, MouseButton button, LPARAM lParam) {
    win->paint.dpi = HostDpi(Hwnd(win));
    float x = PxToDip(&win->paint, GET_X_LPARAM(lParam));
    float y = PxToDip(&win->paint, GET_Y_LPARAM(lParam));
    bool first = win->plat->firstMouse;
    win->plat->firstMouse = false;
    PlatformInput in = InputMouseDown(
        button, x, y, ModsNow(), WindowClickCount(win, x, y, button), first);
    WindowDispatchInput(win, &in);
}

static void MouseUp(Window* win, MouseButton button, LPARAM lParam) {
    float x = PxToDip(&win->paint, GET_X_LPARAM(lParam));
    float y = PxToDip(&win->paint, GET_Y_LPARAM(lParam));
    PlatformInput in =
        InputMouseUp(button, x, y, ModsNow(), WindowCurrentClickCount(win));
    WindowDispatchInput(win, &in);
}

static void MouseMove(Window* win, float x, float y) {
    MouseButton pressed = MouseButton::Left;
    bool any = PressedButton(&pressed);
    PlatformInput in = InputMouseMove(x, y, any, pressed, ModsNow());
    WindowDispatchInput(win, &in);
}

static void MouseExited(Window* win) {
    MouseButton pressed = MouseButton::Left;
    bool any = PressedButton(&pressed);
    PlatformInput in =
        InputMouseExited(win->mouseX, win->mouseY, any, pressed, ModsNow());
    WindowDispatchInput(win, &in);
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
        case WM_MOUSEACTIVATE:
            // The press that follows is the one that activated the window.
            win->plat->firstMouse = true;
            break;
        case WM_RBUTTONDOWN:
            MouseDown(win, MouseButton::Right, lParam);
            return 0;
        case WM_RBUTTONUP:
            MouseUp(win, MouseButton::Right, lParam);
            return 0;
        case WM_MBUTTONDOWN:
            MouseDown(win, MouseButton::Middle, lParam);
            return 0;
        case WM_MBUTTONUP:
            MouseUp(win, MouseButton::Middle, lParam);
            return 0;
        // The two thumb buttons, GPUI's MouseButton::Navigate. Win32 wants
        // TRUE back rather than 0 from these two.
        case WM_XBUTTONDOWN:
            MouseDown(win,
                      GET_XBUTTON_WPARAM(wParam) == XBUTTON1
                          ? MouseButton::NavigateBack
                          : MouseButton::NavigateForward,
                      lParam);
            return TRUE;
        case WM_XBUTTONUP:
            MouseUp(win,
                    GET_XBUTTON_WPARAM(wParam) == XBUTTON1
                        ? MouseButton::NavigateBack
                        : MouseButton::NavigateForward,
                    lParam);
            return TRUE;
        case WM_NCCALCSIZE: {
            // The client title bar owns the top edge: keep the frame the
            // default handler puts on the other three sides but hand the
            // caption band back, so the view paints from y = 0. Maximized,
            // Windows sizes the window past the work area by the frame
            // thickness, so that much of the top inset has to come back or
            // the title bar lands under the screen edge.
            if (!ClientDecorated(win) || wParam == 0) {
                break;
            }
            auto* p = (NCCALCSIZE_PARAMS*)lParam;
            LONG top = p->rgrc[0].top;
            LRESULT r = DefWindowProcW(hwnd, msg, wParam, lParam);
            p->rgrc[0].top = top;
            if (IsZoomed(hwnd)) {
                p->rgrc[0].top += BorderYPx();
            }
            return r;
        }
        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProcW(hwnd, msg, wParam, lParam);
            if (hit != HTCLIENT) {
                return hit;
            }
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(hwnd, &pt);
            // WM_NCCALCSIZE gave the top frame to the client, so the resize
            // band along it is ours to report. The other three sides are
            // still the default handler's, which answered above.
            if (ClientDecorated(win) && !IsZoomed(hwnd) && pt.y < BorderYPx()) {
                RECT rc = {};
                GetClientRect(hwnd, &rc);
                if (pt.x < BorderPx()) {
                    return HTTOPLEFT;
                }
                if (pt.x >= rc.right - BorderPx()) {
                    return HTTOPRIGHT;
                }
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
            MouseMove(win, x, y);
            TRACKMOUSEEVENT tme = {sizeof(tme)};
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
            return 0;
        }
        case WM_MOUSELEAVE:
            MouseExited(win);
            return 0;
        case WM_NCMOUSEMOVE: {
            // The title bar's own cells answer WM_NCHITTEST as HTMINBUTTON,
            // HTMAXBUTTON, HTCLOSE and HTCAPTION, so the pointer over them is
            // non-client and never reaches WM_MOUSEMOVE. Hover still has to
            // follow it. Falls through to the default handler afterwards,
            // which is what puts up the Windows 11 snap layout flyout.
            // Only those four: over a resize border the default handler owns
            // the cursor, and a move event would put the arrow back.
            if (wParam != HTCAPTION && wParam != HTMINBUTTON &&
                wParam != HTMAXBUTTON && wParam != HTCLOSE) {
                break;
            }
            win->paint.dpi = HostDpi(hwnd);
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(hwnd, &pt);
            MouseMove(win, PxToDip(&win->paint, pt.x),
                      PxToDip(&win->paint, pt.y));
            TRACKMOUSEEVENT tme = {sizeof(tme)};
            tme.dwFlags = TME_LEAVE | TME_NONCLIENT;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
            break;
        }
        case WM_NCMOUSELEAVE:
            MouseExited(win);
            break;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
            MouseDown(win, MouseButton::Left, lParam);
            return 0;
        case WM_LBUTTONUP:
            MouseUp(win, MouseButton::Left, lParam);
            return 0;
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
        // Both wheels report in WHEEL_DELTA detents; one notch is 48 DIPs.
        // GPUI would carry that as ScrollDelta::Lines and multiply later --
        // see the note on ScrollWheelEvent. The horizontal wheel counts the
        // other way round, so its sign is flipped to match: positive scrolls
        // the view left, as positive scrolls it up.
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL: {
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(hwnd, &pt);
            float x = PxToDip(&win->paint, pt.x);
            float y = PxToDip(&win->paint, pt.y);
            float delta = (float)GET_WHEEL_DELTA_WPARAM(wParam) /
                          (float)WHEEL_DELTA * 48.f;
            bool horizontal = msg == WM_MOUSEHWHEEL;
            PlatformInput in = InputScrollWheel(x, y, horizontal ? -delta : 0.f,
                                                horizontal ? 0.f : delta, false,
                                                ModsNow(), TouchPhase::Moved);
            WindowDispatchInput(win, &in);
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
    LPCWSTR name = IDC_ARROW;
    if (kind == CursorKind::IBeam) {
        name = IDC_IBEAM;
    } else if (kind == CursorKind::ColResize) {
        name = IDC_SIZEWE;
    }
    win->plat->cursor = LoadCursorW(nullptr, name);
    SetCursor(win->plat->cursor);
}

int PlatDoubleClickMs() {
    return (int)GetDoubleClickTime();
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

Str ClipboardGetText(Arena* a, Window* win) {
    if (!OpenClipboard(Hwnd(win))) {
        return {};
    }
    Str out = {};
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    auto* w = h ? (const WCHAR*)GlobalLock(h) : nullptr;
    if (w) {
        int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr,
                                    nullptr);
        if (n > 1) {
            char* buf = (char*)Alloc(a, n);
            if (buf) {
                WideCharToMultiByte(CP_UTF8, 0, w, -1, buf, n, nullptr,
                                    nullptr);
                out = Str(buf, n - 1);
            }
        }
        GlobalUnlock(h);
    }
    CloseClipboard();
    return out;
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
    if (ClientDecorated(win)) {
        // No caption, but every other part of a normal frame: the thick
        // frame keeps the resize borders, the drop shadow, snapping and the
        // minimize / maximize animations. WM_NCCALCSIZE above then pulls the
        // client area up over the band the caption would have used.
        style = WS_OVERLAPPEDWINDOW & ~WS_CAPTION;
        style |= WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    }
    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);
    WindowClampToDisplay(&dipW, &dipH, sx, sy);
    RECT wr = {0, 0, dipW, dipH};
    AdjustWindowRectEx(&wr, style, FALSE, 0);
    int pxW = wr.right - wr.left;
    int pxH = wr.bottom - wr.top;
    int x = (sx - pxW) / 2;
    int y = (sy - pxH) / 2;
    // -gpui-window: open where the caller asked instead of centred at the
    // caller's size. The numbers are the outer window rect, so they are the
    // same ones MoveWindow and GetWindowRect use.
    WindowGeomRequested(&x, &y, &pxW, &pxH);

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
    if (ClientDecorated(win)) {
        // Creation only asks WM_NCCALCSIZE the wParam == FALSE question,
        // which cannot say where the client area goes. SWP_FRAMECHANGED is
        // what makes Windows ask the real one, so the caption band the
        // handler above reclaims is gone before the window is ever shown.
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                         SWP_NOACTIVATE);
    }
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
    argc = gpui::GpuiTakeRuntimeArgs(argc, argv);
    return GpuiMain(argc, argv);
}
