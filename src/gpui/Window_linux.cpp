/* The X11 window: event loop, chrome routing, timers, clipboard, and the
   process entry point. The mirror of Window_win.cpp; everything either of
   them decides is delegated to WindowCommon.cpp.

   Like the Direct2D target, the cairo target runs at 96 dpi, so one DIP is
   one device pixel and no coordinate here is scaled. */

#include "gpui/Platform.h"
#include "gpui/Paint.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include <locale.h>
#include <poll.h>
#include <time.h>

namespace gpui {

// X11's `Window` is an unsigned long; inside namespace gpui the unqualified
// name is ours, so the X one is always spelled ::Window.
using XWindow = ::Window;

struct PlatWindow {
    XWindow xwin = 0;
    cairo_surface_t* surf = nullptr; // the window itself
    cairo_surface_t* back = nullptr; // what a frame is drawn into
    XIC xic = nullptr;
    int pxW = 0;
    int pxH = 0;
    bool dirty = true;
    // Monotonic deadline for the next tick; 0 when the timer is off.
    double nextTick = 0;
    // -1, or the _NET_WM_MOVERESIZE direction the pointer is currently over.
    // WindowMouseMove only asks for arrow or I-beam, so the edge cursor has
    // to be put back by hand once the pointer leaves the band, and put back
    // up again whenever that arrow/I-beam choice changed underneath it.
    int edge = -1;
    CursorKind edgeUnder = CursorKind::Arrow;
};

// One display per process. GPUI's App is a singleton in practice, and an X
// connection is the one piece of state every window here shares.
static Display* gDpy = nullptr;
static int gScreen = 0;
static XWindow gRoot = 0;
static XIM gXim = nullptr;
static Str gClipboard = {};

static Atom aWmDeleteWindow, aWmProtocols, aNetWmName, aUtf8String;
static Atom aNetWmState, aNetWmStateMaxVert, aNetWmStateMaxHorz;
static Atom aNetWmMoveResize, aMotifWmHints, aGtkShowWindowMenu;
static Atom aClipboard, aTargets;

double TimeNow() {
    static bool started = false;
    static struct timespec start = {};
    struct timespec now = {};
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (!started) {
        start = now;
        started = true;
    }
    return (double)(now.tv_sec - start.tv_sec) +
           (double)(now.tv_nsec - start.tv_nsec) / 1e9;
}

static Window* FindWindow(App* app, XWindow xwin) {
    if (!app) {
        return nullptr;
    }
    for (int i = 0; i < app->windows.len; i++) {
        Window* w = app->windows[i];
        if (w->plat && w->plat->xwin == xwin) {
            return w;
        }
    }
    return nullptr;
}

// ─── drawing ──────────────────────────────────────────────────────────────

static void EnsureSurfaces(Window* win) {
    PlatWindow* pw = win->plat;
    if (!pw || pw->pxW <= 0 || pw->pxH <= 0) {
        return;
    }
    if (!pw->surf) {
        pw->surf = cairo_xlib_surface_create(
            gDpy, pw->xwin, DefaultVisual(gDpy, gScreen), pw->pxW, pw->pxH);
    } else {
        cairo_xlib_surface_set_size(pw->surf, pw->pxW, pw->pxH);
    }
    if (pw->back) {
        if (cairo_image_surface_get_width(pw->back) != pw->pxW ||
            cairo_image_surface_get_height(pw->back) != pw->pxH) {
            cairo_surface_destroy(pw->back);
            pw->back = nullptr;
        }
    }
    if (!pw->back) {
        pw->back =
            cairo_image_surface_create(CAIRO_FORMAT_RGB24, pw->pxW, pw->pxH);
    }
}

static void Redraw(Window* win) {
    PlatWindow* pw = win->plat;
    if (!pw) {
        return;
    }
    pw->dirty = false;
    EnsureSurfaces(win);
    if (!pw->surf || !pw->back) {
        return;
    }
    win->paint.dpi = 96;
    WindowDrawFrame(win, pw->back, pw->pxW, pw->pxH, (float)pw->pxW,
                    (float)pw->pxH);
    // Blit the finished frame in one operation, so a slow frame never shows
    // half-drawn.
    cairo_t* cr = cairo_create(pw->surf);
    cairo_set_source_surface(cr, pw->back, 0, 0);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);
    cairo_destroy(cr);
    cairo_surface_flush(pw->surf);
    XFlush(gDpy);
}

// ─── window state ─────────────────────────────────────────────────────────

static bool ReadMaximized(Window* win) {
    PlatWindow* pw = win->plat;
    if (!pw) {
        return false;
    }
    Atom type = 0;
    int format = 0;
    unsigned long n = 0, after = 0;
    unsigned char* data = nullptr;
    if (XGetWindowProperty(gDpy, pw->xwin, aNetWmState, 0, 32, False, XA_ATOM,
                           &type, &format, &n, &after, &data) != Success) {
        return false;
    }
    bool vert = false;
    bool horz = false;
    if (data) {
        auto* atoms = (Atom*)data;
        for (unsigned long i = 0; i < n; i++) {
            if (atoms[i] == aNetWmStateMaxVert) {
                vert = true;
            }
            if (atoms[i] == aNetWmStateMaxHorz) {
                horz = true;
            }
        }
        XFree(data);
    }
    return vert && horz;
}

static void SendWmState(Window* win, Atom a, Atom b, int action) {
    PlatWindow* pw = win->plat;
    if (!pw) {
        return;
    }
    XEvent ev = {};
    ev.type = ClientMessage;
    ev.xclient.window = pw->xwin;
    ev.xclient.message_type = aNetWmState;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = action; // 0 remove, 1 add, 2 toggle
    ev.xclient.data.l[1] = (long)a;
    ev.xclient.data.l[2] = (long)b;
    ev.xclient.data.l[3] = 1; // normal application
    XSendEvent(gDpy, gRoot, False,
               SubstructureNotifyMask | SubstructureRedirectMask, &ev);
    XFlush(gDpy);
}

static void SetUndecorated(XWindow xwin) {
    // The Motif hint is what every reasonable window manager still honours
    // for "no frame, but a normal window".
    struct MotifHints {
        unsigned long flags;
        unsigned long functions;
        unsigned long decorations;
        long input_mode;
        unsigned long status;
    };
    MotifHints hints = {};
    hints.flags = 2; // MWM_HINTS_DECORATIONS
    hints.decorations = 0;
    XChangeProperty(gDpy, xwin, aMotifWmHints, aMotifWmHints, 32,
                    PropModeReplace, (unsigned char*)&hints, 5);
}

// ─── input translation ────────────────────────────────────────────────────

static int KeyFor(KeySym ks) {
    switch (ks) {
        case XK_BackSpace:
            return KeyBack;
        case XK_Tab:
        case XK_ISO_Left_Tab:
            return KeyTab;
        case XK_Return:
        case XK_KP_Enter:
            return KeyReturn;
        case XK_Shift_L:
        case XK_Shift_R:
            return KeyShift;
        case XK_Control_L:
        case XK_Control_R:
            return KeyControl;
        case XK_Alt_L:
        case XK_Alt_R:
            return KeyMenu;
        case XK_Escape:
            return KeyEscape;
        case XK_space:
            return KeySpace;
        case XK_Prior:
            return KeyPageUp;
        case XK_Next:
            return KeyPageDown;
        case XK_End:
            return KeyEnd;
        case XK_Home:
            return KeyHome;
        case XK_Left:
            return KeyLeft;
        case XK_Up:
            return KeyUp;
        case XK_Right:
            return KeyRight;
        case XK_Down:
            return KeyDown;
        case XK_Delete:
            return KeyDelete;
        default:
            break;
    }
    // Letters and digits carry their ASCII uppercase code, as VK_* does.
    if (ks >= XK_a && ks <= XK_z) {
        return (int)(ks - XK_a) + 'A';
    }
    if (ks >= XK_A && ks <= XK_Z) {
        return (int)(ks - XK_A) + 'A';
    }
    if (ks >= XK_0 && ks <= XK_9) {
        return (int)(ks - XK_0) + '0';
    }
    return 0;
}

// Decode one UTF-8 codepoint; returns how many bytes it used.
static int Utf8Next(const char* s, int len, uint32_t* out) {
    if (len <= 0) {
        return 0;
    }
    auto b = (const unsigned char*)s;
    if (b[0] < 0x80) {
        *out = b[0];
        return 1;
    }
    if ((b[0] & 0xe0) == 0xc0 && len >= 2) {
        *out = ((uint32_t)(b[0] & 0x1f) << 6) | (b[1] & 0x3f);
        return 2;
    }
    if ((b[0] & 0xf0) == 0xe0 && len >= 3) {
        *out = ((uint32_t)(b[0] & 0x0f) << 12) |
               ((uint32_t)(b[1] & 0x3f) << 6) | (b[2] & 0x3f);
        return 3;
    }
    if ((b[0] & 0xf8) == 0xf0 && len >= 4) {
        *out = ((uint32_t)(b[0] & 0x07) << 18) |
               ((uint32_t)(b[1] & 0x3f) << 12) |
               ((uint32_t)(b[2] & 0x3f) << 6) | (b[3] & 0x3f);
        return 4;
    }
    *out = b[0];
    return 1;
}

static void OnKeyPress(Window* win, XKeyEvent* ke) {
    PlatWindow* pw = win->plat;
    char buf[64] = {};
    KeySym ks = 0;
    int n = 0;
    if (pw && pw->xic) {
        Status st = 0;
        n = Xutf8LookupString(pw->xic, ke, buf, (int)sizeof(buf) - 1, &ks, &st);
        if (st == XLookupNone) {
            return;
        }
        if (st != XLookupChars && st != XLookupBoth) {
            n = 0;
        }
    } else {
        n = XLookupString(ke, buf, (int)sizeof(buf) - 1, &ks, nullptr);
    }
    bool shift = (ke->state & ShiftMask) != 0;
    bool ctrl = (ke->state & ControlMask) != 0;
    bool alt = (ke->state & Mod1Mask) != 0;

    int key = KeyFor(ks);
    if (key) {
        WindowKeyDown(win, key, shift, ctrl, alt);
    }
    // Windows delivers backspace as WM_CHAR 8, and the bound LineInput edits
    // on that; X11 only reports the keysym, so raise it here.
    if (key == KeyBack) {
        WindowChar(win, 8, ctrl, alt);
        return;
    }
    // Backspace and Return arrive as both a key and a control character on
    // X11; the text path only wants real typing.
    if (n <= 0 || ctrl || alt || key == KeyReturn || key == KeyTab ||
        key == KeyEscape) {
        return;
    }
    int i = 0;
    while (i < n) {
        uint32_t cp = 0;
        int adv = Utf8Next(buf + i, n - i, &cp);
        if (adv <= 0) {
            break;
        }
        i += adv;
        if (cp >= 32 && cp != 127) {
            WindowChar(win, cp, ctrl, alt);
        }
    }
}

// _NET_WM_MOVERESIZE: the window manager takes the pointer grab and runs the
// drag. Directions 0..7 go clockwise from the top-left corner; 8 is a move.
static const int kMoveResizeMove = 8;

static void StartMoveResize(Window* win, int rootX, int rootY, int dir) {
    PlatWindow* pw = win->plat;
    if (!pw) {
        return;
    }
    XUngrabPointer(gDpy, CurrentTime);
    XEvent ev = {};
    ev.type = ClientMessage;
    ev.xclient.window = pw->xwin;
    ev.xclient.message_type = aNetWmMoveResize;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = rootX;
    ev.xclient.data.l[1] = rootY;
    ev.xclient.data.l[2] = dir;
    ev.xclient.data.l[3] = Button1;
    ev.xclient.data.l[4] = 1;
    XSendEvent(gDpy, gRoot, False,
               SubstructureNotifyMask | SubstructureRedirectMask, &ev);
    XFlush(gDpy);
}

static void StartMoveDrag(Window* win, int rootX, int rootY) {
    StartMoveResize(win, rootX, rootY, kMoveResizeMove);
}

// _GTK_SHOW_WINDOW_MENU: what a right click on the title bar of a
// client-decorated window asks the window manager for. Windows gets the same
// menu from DefWindowProc on WM_NCRBUTTONUP over HTCAPTION.
static void ShowWindowMenu(Window* win, int rootX, int rootY) {
    PlatWindow* pw = win->plat;
    if (!pw) {
        return;
    }
    XUngrabPointer(gDpy, CurrentTime);
    XEvent ev = {};
    ev.type = ClientMessage;
    ev.xclient.window = pw->xwin;
    ev.xclient.message_type = aGtkShowWindowMenu;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = 0; // the device, which we do not track
    ev.xclient.data.l[1] = rootX;
    ev.xclient.data.l[2] = rootY;
    XSendEvent(gDpy, gRoot, False,
               SubstructureNotifyMask | SubstructureRedirectMask, &ev);
    XFlush(gDpy);
}

// An undecorated window has no frame to grab, so the outer band of the client
// area is the resize handle — the same job WM_NCHITTEST does on Windows.
static const int kResizeBand = 6;

static bool ClientDecorated(Window* win) {
    return win->opts.clientTitleBar || win->opts.borderless;
}

static int ResizeEdge(Window* win, int x, int y) {
    PlatWindow* pw = win->plat;
    if (!pw || !ClientDecorated(win) || win->maximized) {
        return -1;
    }
    bool l = x < kResizeBand;
    bool r = x >= pw->pxW - kResizeBand;
    bool t = y < kResizeBand;
    bool b = y >= pw->pxH - kResizeBand;
    if (t) {
        return l ? 0 : r ? 2 : 1;
    }
    if (b) {
        return l ? 6 : r ? 4 : 5;
    }
    if (l) {
        return 7;
    }
    return r ? 3 : -1;
}

static void SetEdgeCursor(Window* win, int dir) {
    PlatWindow* pw = win->plat;
    if (!pw || (pw->edge == dir && pw->edgeUnder == win->cursor)) {
        return;
    }
    pw->edge = dir;
    pw->edgeUnder = win->cursor;
    if (dir < 0) {
        // WindowMouseMove decided on this one before the band overrode it.
        PlatSetCursor(win, win->cursor);
        return;
    }
    static const unsigned kShapes[8] = {XC_top_left_corner,     XC_top_side,
                                        XC_top_right_corner,    XC_right_side,
                                        XC_bottom_right_corner, XC_bottom_side,
                                        XC_bottom_left_corner,  XC_left_side};
    // The server owns cursors; one per shape is all this needs.
    static ::Cursor cache[8] = {};
    if (!cache[dir]) {
        cache[dir] = XCreateFontCursor(gDpy, kShapes[dir]);
    }
    XDefineCursor(gDpy, pw->xwin, cache[dir]);
    XFlush(gDpy);
}

// ─── clipboard ────────────────────────────────────────────────────────────

void PlatSetCursor(Window* win, CursorKind kind) {
    if (!win || !win->plat || !gDpy) {
        return;
    }
    // The server owns these; two per process is all this needs.
    static ::Cursor arrow = 0;
    static ::Cursor ibeam = 0;
    if (!arrow) {
        arrow = XCreateFontCursor(gDpy, XC_left_ptr);
    }
    if (!ibeam) {
        ibeam = XCreateFontCursor(gDpy, XC_xterm);
    }
    XDefineCursor(gDpy, win->plat->xwin,
                  kind == CursorKind::IBeam ? ibeam : arrow);
    XFlush(gDpy);
}

int PlatDoubleClickMs() {
    // X11 keeps no double-click interval of its own; 400 ms is what GTK and
    // Qt default to.
    return 400;
}

void ClipboardSetText(Window* win, Str text) {
    if (!win || !win->plat || !text.s || text.len <= 0) {
        return;
    }
    if (gClipboard.s) {
        StrFree(gClipboard);
    }
    gClipboard = StrDup(text);
    XSetSelectionOwner(gDpy, aClipboard, win->plat->xwin, CurrentTime);
    XFlush(gDpy);
}

static void OnSelectionRequest(XSelectionRequestEvent* req) {
    XEvent resp = {};
    resp.xselection.type = SelectionNotify;
    resp.xselection.requestor = req->requestor;
    resp.xselection.selection = req->selection;
    resp.xselection.target = req->target;
    resp.xselection.time = req->time;
    resp.xselection.property = None;

    Atom prop = req->property ? req->property : req->target;
    if (req->target == aTargets) {
        Atom targets[2] = {aTargets, aUtf8String};
        XChangeProperty(gDpy, req->requestor, prop, XA_ATOM, 32,
                        PropModeReplace, (unsigned char*)targets, 2);
        resp.xselection.property = prop;
    } else if ((req->target == aUtf8String || req->target == XA_STRING) &&
               gClipboard.s) {
        XChangeProperty(gDpy, req->requestor, prop, req->target, 8,
                        PropModeReplace, (unsigned char*)gClipboard.s,
                        gClipboard.len);
        resp.xselection.property = prop;
    }
    XSendEvent(gDpy, req->requestor, False, 0, &resp);
    XFlush(gDpy);
}

// ─── event dispatch ───────────────────────────────────────────────────────

static void DestroyPlatWindow(Window* win) {
    PlatWindow* pw = win->plat;
    if (!pw) {
        return;
    }
    if (pw->back) {
        cairo_surface_destroy(pw->back);
    }
    if (pw->surf) {
        cairo_surface_destroy(pw->surf);
    }
    if (pw->xic) {
        XDestroyIC(pw->xic);
    }
    XWindow xwin = pw->xwin;
    delete pw;
    WindowClosed(win);
    if (xwin) {
        XDestroyWindow(gDpy, xwin);
    }
}

static void HandleEvent(App* app, XEvent* ev) {
    if (ev->type == SelectionRequest) {
        OnSelectionRequest(&ev->xselectionrequest);
        return;
    }
    if (ev->type == SelectionClear) {
        if (gClipboard.s) {
            StrFree(gClipboard);
            gClipboard = {};
        }
        return;
    }
    XWindow xwin = ev->xany.window;
    Window* win = FindWindow(app, xwin);
    if (!win) {
        return;
    }
    PlatWindow* pw = win->plat;
    switch (ev->type) {
        case Expose:
            if (ev->xexpose.count == 0) {
                pw->dirty = true;
            }
            break;
        case ConfigureNotify:
            if (ev->xconfigure.width != pw->pxW || ev->xconfigure
                                                           .height != pw->pxH) {
                pw->pxW = ev->xconfigure.width;
                pw->pxH = ev->xconfigure.height;
                pw->dirty = true;
            }
            break;
        case PropertyNotify:
            if (ev->xproperty.atom == aNetWmState) {
                win->maximized = ReadMaximized(win);
            }
            break;
        case KeyPress:
            OnKeyPress(win, &ev->xkey);
            break;
        case MotionNotify:
            WindowMouseMove(win, (float)ev->xmotion.x, (float)ev->xmotion.y);
            SetEdgeCursor(win, ResizeEdge(win, ev->xmotion.x, ev->xmotion.y));
            break;
        case LeaveNotify:
            WindowMouseLeave(win);
            // Whatever the pointer picks up outside, the band has to claim
            // the cursor again the next time it comes back.
            pw->edge = -1;
            break;
        case ButtonPress: {
            float x = (float)ev->xbutton.x;
            float y = (float)ev->xbutton.y;
            unsigned b = ev->xbutton.button;
            if (b == Button4 || b == Button5) {
                // One notch is 48 DIPs, the same step the Windows window uses.
                WindowWheel(win, x, y, b == Button4 ? 48.f : -48.f);
                break;
            }
            if (b == Button3) {
                // The title bar's drag region belongs to the window manager,
                // the way it does for a server-decorated one.
                if (WindowChromeHit(win, x, y) == ClickWinCaption) {
                    ShowWindowMenu(win, ev->xbutton.x_root, ev->xbutton.y_root);
                    break;
                }
                WindowMouseDown(win, x, y, 2, WindowClickCount(win, x, y, 2));
                break;
            }
            if (b != Button1) {
                break;
            }
            // Before the chrome, because the chrome needs the answer: the
            // caption drags on the first press and zooms on the second, and
            // handing the drag to the window manager first would swallow it.
            int clicks = WindowClickCount(win, x, y, 1);
            // The resize band and the custom chrome are claimed before the
            // element tree sees the press, the way WM_NCHITTEST takes both
            // on Windows.
            int edge = ResizeEdge(win, ev->xbutton.x, ev->xbutton.y);
            if (edge >= 0) {
                StartMoveResize(win, ev->xbutton.x_root, ev->xbutton.y_root,
                                edge);
                break;
            }
            int chrome = WindowChromeHit(win, x, y);
            if (chrome == ClickWinMin) {
                AppMinimize(win);
                break;
            }
            if (chrome == ClickWinMax) {
                AppToggleMaximize(win);
                break;
            }
            if (chrome == ClickWinClose) {
                AppClose(win);
                break;
            }
            if (chrome == ClickWinCaption) {
                if (clicks == 2) {
                    AppToggleMaximize(win);
                } else {
                    StartMoveDrag(win, ev->xbutton.x_root, ev->xbutton.y_root);
                }
                break;
            }
            WindowMouseDown(win, x, y, 1, clicks);
            break;
        }
        case ButtonRelease:
            if (ev->xbutton.button == Button1) {
                WindowMouseUp(win, (float)ev->xbutton.x, (float)ev->xbutton.y,
                              1);
            }
            break;
        case ClientMessage:
            if (ev->xclient.message_type == aWmProtocols &&
                (Atom)ev->xclient.data.l[0] == aWmDeleteWindow) {
                DestroyPlatWindow(win);
            }
            break;
        default:
            break;
    }
}

// ─── window commands ──────────────────────────────────────────────────────

void AppQuit(Window* win) {
    if (win && win->plat) {
        DestroyPlatWindow(win);
    }
}

void AppInvalidate(Window* win) {
    if (win && win->plat) {
        win->plat->dirty = true;
    }
}

void AppMinimize(Window* win) {
    if (win && win->plat) {
        XIconifyWindow(gDpy, win->plat->xwin, gScreen);
        XFlush(gDpy);
    }
}

void AppToggleMaximize(Window* win) {
    if (win && win->plat) {
        SendWmState(win, aNetWmStateMaxVert, aNetWmStateMaxHorz, 2);
    }
}

void AppDrag(Window* win) {
    if (!win || !win->plat) {
        return;
    }
    XWindow child = 0;
    int rx = 0, ry = 0, wx = 0, wy = 0;
    unsigned mask = 0;
    XWindow rootRet = 0;
    XQueryPointer(gDpy, win->plat->xwin, &rootRet, &child, &rx, &ry, &wx, &wy,
                  &mask);
    StartMoveDrag(win, rx, ry);
}

void AppSetTitle(Window* win, Str title) {
    if (!win || !win->plat || !title.s) {
        return;
    }
    XWindow xwin = win->plat->xwin;
    // _NET_WM_NAME is the UTF-8 one modern window managers read; WM_NAME is
    // the Latin-1 fallback.
    XChangeProperty(gDpy, xwin, aNetWmName, aUtf8String, 8, PropModeReplace,
                    (unsigned char*)title.s, title.len);
    Str z = StrDup(title);
    if (z.s) {
        XStoreName(gDpy, xwin, z.s);
        StrFree(z);
    }
    XFlush(gDpy);
}

void PlatSetTimer(Window* win, int ms) {
    if (!win || !win->plat) {
        return;
    }
    win->plat->nextTick = ms > 0 ? TimeNow() + ms / 1000.0 : 0;
}

// ─── app lifecycle ────────────────────────────────────────────────────────

bool PlatInit(App* app) {
    (void)app;
    if (gDpy) {
        return true;
    }
    setlocale(LC_ALL, "");
    XSetLocaleModifiers("");
    gDpy = XOpenDisplay(nullptr);
    if (!gDpy) {
        logf("XOpenDisplay failed: no DISPLAY?");
        return false;
    }
    gScreen = DefaultScreen(gDpy);
    gRoot = RootWindow(gDpy, gScreen);
    gXim = XOpenIM(gDpy, nullptr, nullptr, nullptr);

    aWmProtocols = XInternAtom(gDpy, "WM_PROTOCOLS", False);
    aWmDeleteWindow = XInternAtom(gDpy, "WM_DELETE_WINDOW", False);
    aNetWmName = XInternAtom(gDpy, "_NET_WM_NAME", False);
    aUtf8String = XInternAtom(gDpy, "UTF8_STRING", False);
    aNetWmState = XInternAtom(gDpy, "_NET_WM_STATE", False);
    aNetWmStateMaxVert =
        XInternAtom(gDpy, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    aNetWmStateMaxHorz =
        XInternAtom(gDpy, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    aNetWmMoveResize = XInternAtom(gDpy, "_NET_WM_MOVERESIZE", False);
    aMotifWmHints = XInternAtom(gDpy, "_MOTIF_WM_HINTS", False);
    aGtkShowWindowMenu = XInternAtom(gDpy, "_GTK_SHOW_WINDOW_MENU", False);
    aClipboard = XInternAtom(gDpy, "CLIPBOARD", False);
    aTargets = XInternAtom(gDpy, "TARGETS", False);
    return true;
}

void PlatShutdown(App* app) {
    (void)app;
    if (gClipboard.s) {
        StrFree(gClipboard);
        gClipboard = {};
    }
    if (gXim) {
        XCloseIM(gXim);
        gXim = nullptr;
    }
    if (gDpy) {
        XCloseDisplay(gDpy);
        gDpy = nullptr;
    }
}

Window* WindowOpen(App* app, Str title, int dipW, int dipH, WinOpts opts) {
    if (!gDpy) {
        return nullptr;
    }
    Window* win = WindowAlloc(app, opts);
    if (!win) {
        return nullptr;
    }
    int sw = DisplayWidth(gDpy, gScreen);
    int sh = DisplayHeight(gDpy, gScreen);
    WindowClampToDisplay(&dipW, &dipH, sw, sh);

    auto* pw = new PlatWindow();
    pw->pxW = dipW;
    pw->pxH = dipH;

    int x = (sw - dipW) / 2;
    int y = (sh - dipH) / 2;

    XSetWindowAttributes attrs = {};
    attrs.background_pixel = BlackPixel(gDpy, gScreen);
    attrs.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                       ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                       LeaveWindowMask | StructureNotifyMask |
                       PropertyChangeMask | FocusChangeMask;
    pw->xwin = XCreateWindow(gDpy, gRoot, x, y, (unsigned)dipW, (unsigned)dipH,
                             0, CopyFromParent, InputOutput, CopyFromParent,
                             CWBackPixel | CWEventMask, &attrs);
    if (!pw->xwin) {
        delete pw;
        return nullptr;
    }
    win->plat = pw;

    XSetWMProtocols(gDpy, pw->xwin, &aWmDeleteWindow, 1);
    if (opts.borderless || opts.clientTitleBar) {
        SetUndecorated(pw->xwin);
    }
    AppSetTitle(win, title);

    if (gXim) {
        pw->xic = XCreateIC(
            gXim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
            XNClientWindow, pw->xwin, XNFocusWindow, pw->xwin, nullptr);
    }

    XMapWindow(gDpy, pw->xwin);
    XFlush(gDpy);
    PlatSetTimer(win, WindowTimerMs(win));
    return win;
}

int AppRun(App* app) {
    if (!app || !gDpy) {
        return 1;
    }
    int fd = ConnectionNumber(gDpy);
    while (AppAnyWindowOpen(app)) {
        while (XPending(gDpy) > 0) {
            XEvent ev = {};
            XNextEvent(gDpy, &ev);
            if (XFilterEvent(&ev, None)) {
                continue;
            }
            HandleEvent(app, &ev);
        }
        if (!AppAnyWindowOpen(app)) {
            break;
        }

        for (int i = 0; i < app->windows.len; i++) {
            Window* w = app->windows[i];
            if (w->plat && w->plat->dirty) {
                Redraw(w);
            }
        }

        // Sleep until the next tick, or until X has something to say.
        double now = TimeNow();
        double waitS = 1.0;
        bool anyDirty = false;
        for (int i = 0; i < app->windows.len; i++) {
            Window* w = app->windows[i];
            if (!w->plat) {
                continue;
            }
            if (w->plat->dirty) {
                anyDirty = true;
            }
            if (w->plat->nextTick > 0) {
                double d = w->plat->nextTick - now;
                if (d < waitS) {
                    waitS = d;
                }
            }
        }
        if (!anyDirty && XPending(gDpy) == 0) {
            int timeoutMs = waitS <= 0 ? 0 : (int)(waitS * 1000.0);
            struct pollfd pfd = {fd, POLLIN, 0};
            poll(&pfd, 1, timeoutMs);
        }

        now = TimeNow();
        for (int i = 0; i < app->windows.len; i++) {
            Window* w = app->windows[i];
            if (!w->plat || w->plat->nextTick <= 0) {
                continue;
            }
            if (now >= w->plat->nextTick) {
                // WindowTimerTick re-arms through PlatSetTimer.
                WindowTimerTick(w);
            }
        }
    }
    return app->exitCode;
}

} // namespace gpui

// The process entry point. Examples implement GpuiMain(argc, argv).
int main(int argc, char** argv) {
    // Strip the -gpui-* flags here too, so an example parses the same argv on
    // every platform. -gpui-window itself is only honoured on Windows, where
    // the screenshot harness runs.
    argc = gpui::GpuiTakeRuntimeArgs(argc, argv);
    return GpuiMain(argc, argv);
}
