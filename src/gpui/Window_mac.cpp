/* The Cocoa window: event pump, chrome routing, timers, clipboard, and the
   process entry point. The mirror of Window_win.cpp and Window_linux.cpp;
   everything any of them decides is delegated to WindowCommon.cpp.

   Objective-C++ under ARC (-x objective-c++ -fobjc-arc). The view is flipped,
   so a frame is drawn in points with the origin at the top left. One point is
   one DIP, which on a Retina display means the backing store is 2x and the
   drawing comes out crisp for free. */

#include "gpui/Platform.h"
#include "gpui/Paint.h"

#import <Cocoa/Cocoa.h>

#include <time.h>

@class GpuiView;
@class GpuiWindowDelegate;

namespace gpui {

struct PlatWindow {
    NSWindow* window = nil;
    GpuiView* view = nil;
    // The window does not retain its delegate; this reference is what keeps
    // it alive.
    GpuiWindowDelegate* delegate = nil;
    bool dirty = true;
    // Monotonic deadline for the next tick; 0 when the timer is off.
    double nextTick = 0;
};

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

// Defined below, once NSEvent is in scope for the whole file.
void WindowMacKeyDown(Window* win, NSEvent* event);

} // namespace gpui

// ─── the view ─────────────────────────────────────────────────────────────

@interface GpuiView : NSView {
  @public
    gpui::Window* win;
}
@end

@implementation GpuiView

- (BOOL)isFlipped {
    return YES;
}
- (BOOL)acceptsFirstResponder {
    return YES;
}
- (BOOL)acceptsFirstMouse:(NSEvent*)event {
    (void)event;
    return YES;
}

- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    if (!win) {
        return;
    }
    CGContextRef cg =
        (CGContextRef)[[NSGraphicsContext currentContext] CGContext];
    NSRect b = [self bounds];
    NSRect px = [self convertRectToBacking:b];
    win->paint.dpi = 96;
    gpui::WindowDrawFrame(win, cg, (int)px.size.width, (int)px.size.height,
                          (float)b.size.width, (float)b.size.height);
}

// A tracking area is what turns on mouseMoved / mouseExited.
- (void)updateTrackingAreas {
    for (NSTrackingArea* a in [self trackingAreas]) {
        [self removeTrackingArea:a];
    }
    NSTrackingAreaOptions opts =
        NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited |
        NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect;
    NSTrackingArea* area = [[NSTrackingArea alloc] initWithRect:[self bounds]
                                                        options:opts
                                                          owner:self
                                                       userInfo:nil];
    [self addTrackingArea:area];
    [super updateTrackingAreas];
}

- (NSPoint)gpuiPoint:(NSEvent*)event {
    return [self convertPoint:[event locationInWindow] fromView:nil];
}

- (void)mouseMoved:(NSEvent*)event {
    NSPoint p = [self gpuiPoint:event];
    gpui::WindowMouseMove(win, (float)p.x, (float)p.y);
}
- (void)mouseDragged:(NSEvent*)event {
    [self mouseMoved:event];
}
- (void)mouseExited:(NSEvent*)event {
    (void)event;
    gpui::WindowMouseLeave(win);
}

- (void)mouseDown:(NSEvent*)event {
    NSPoint p = [self gpuiPoint:event];
    float x = (float)p.x;
    float y = (float)p.y;
    // The custom chrome is claimed before the element tree sees the press,
    // the way WM_NCHITTEST takes it on Windows.
    int chrome = gpui::WindowChromeHit(win, x, y);
    if (chrome == gpui::ClickWinMin) {
        gpui::AppMinimize(win);
        return;
    }
    if (chrome == gpui::ClickWinMax) {
        gpui::AppToggleMaximize(win);
        return;
    }
    if (chrome == gpui::ClickWinClose) {
        gpui::AppClose(win);
        return;
    }
    if (chrome == gpui::ClickWinCaption) {
        [[self window] performWindowDragWithEvent:event];
        return;
    }
    if ([event clickCount] == 2) {
        gpui::WindowDoubleClick(win, x, y);
        return;
    }
    gpui::WindowMouseDown(win, x, y, 1);
}

- (void)mouseUp:(NSEvent*)event {
    NSPoint p = [self gpuiPoint:event];
    gpui::WindowMouseUp(win, (float)p.x, (float)p.y, 1);
}

- (void)rightMouseDown:(NSEvent*)event {
    NSPoint p = [self gpuiPoint:event];
    gpui::WindowMouseDown(win, (float)p.x, (float)p.y, 2);
}

- (void)scrollWheel:(NSEvent*)event {
    NSPoint p = [self gpuiPoint:event];
    // A line of scroll is 48 DIPs, the step the other two windows use; a
    // precise (trackpad) delta is already in points.
    float delta = (float)[event scrollingDeltaY];
    if (![event hasPreciseScrollingDeltas]) {
        delta *= 48.f;
    }
    gpui::WindowWheel(win, (float)p.x, (float)p.y, delta);
}

- (void)keyDown:(NSEvent*)event {
    gpui::WindowMacKeyDown(win, event);
}

- (void)keyUp:(NSEvent*)event {
    (void)event;
}

// Cocoa beeps on an unhandled key equivalent; the app handles its own keys.
- (BOOL)performKeyEquivalent:(NSEvent*)event {
    (void)event;
    return NO;
}

@end

// ─── the window delegate ──────────────────────────────────────────────────

@interface GpuiWindowDelegate : NSObject <NSWindowDelegate> {
  @public
    gpui::Window* win;
}
@end

@implementation GpuiWindowDelegate

- (void)windowWillClose:(NSNotification*)note {
    (void)note;
    gpui::WindowClosed(win);
}

- (void)windowDidResize:(NSNotification*)note {
    (void)note;
    gpui::AppInvalidate(win);
}

- (void)windowDidChangeBackingProperties:(NSNotification*)note {
    (void)note;
    gpui::AppInvalidate(win);
}

@end

namespace gpui {

// ─── keys ─────────────────────────────────────────────────────────────────

static int KeyFor(unichar c) {
    switch (c) {
        case NSUpArrowFunctionKey:
            return KeyUp;
        case NSDownArrowFunctionKey:
            return KeyDown;
        case NSLeftArrowFunctionKey:
            return KeyLeft;
        case NSRightArrowFunctionKey:
            return KeyRight;
        case NSHomeFunctionKey:
            return KeyHome;
        case NSEndFunctionKey:
            return KeyEnd;
        case NSPageUpFunctionKey:
            return KeyPageUp;
        case NSPageDownFunctionKey:
            return KeyPageDown;
        case NSDeleteFunctionKey:
            return KeyDelete;
        case 0x7f: // the key labelled Delete on a Mac keyboard
            return KeyBack;
        case '\r':
        case 0x03:
            return KeyReturn;
        case '\t':
        case 0x19: // back-tab, what Shift-Tab produces
            return KeyTab;
        case 0x1b:
            return KeyEscape;
        case ' ':
            return KeySpace;
        default:
            break;
    }
    if (c >= 'a' && c <= 'z') {
        return (int)(c - 'a') + 'A';
    }
    if (c >= 'A' && c <= 'Z') {
        return (int)c;
    }
    if (c >= '0' && c <= '9') {
        return (int)c;
    }
    return 0;
}

void WindowMacKeyDown(Window* win, NSEvent* event) {
    if (!win) {
        return;
    }
    NSEventModifierFlags mods = [event modifierFlags];
    bool shift = (mods & NSEventModifierFlagShift) != 0;
    // Command is the Mac's shortcut modifier, so it and Control both land on
    // `ctrl` — that is what a Ctrl-C handler means on either platform.
    bool ctrl =
        (mods & (NSEventModifierFlagControl | NSEventModifierFlagCommand)) != 0;
    bool alt = (mods & NSEventModifierFlagOption) != 0;

    NSString* bare = [event charactersIgnoringModifiers];
    unichar first = [bare length] > 0 ? [bare characterAtIndex:0] : 0;
    int key = KeyFor(first);
    if (key) {
        WindowKeyDown(win, key, shift, ctrl, alt);
    }
    // Backspace arrives as a key only; the bound LineInput edits on the
    // control code the Windows window delivers through WM_CHAR.
    if (key == KeyBack) {
        WindowChar(win, 8, ctrl, alt);
        return;
    }
    if (ctrl || alt || key == KeyReturn || key == KeyTab || key == KeyEscape) {
        return;
    }
    NSString* text = [event characters];
    NSUInteger n = [text length];
    for (NSUInteger i = 0; i < n; i++) {
        unichar u = [text characterAtIndex:i];
        uint32_t cp = u;
        // Recombine a surrogate pair before handing over a codepoint.
        if (u >= 0xd800 && u <= 0xdbff && i + 1 < n) {
            unichar lo = [text characterAtIndex:i + 1];
            if (lo >= 0xdc00 && lo <= 0xdfff) {
                cp = 0x10000 + ((uint32_t)(u - 0xd800) << 10) + (lo - 0xdc00);
                i++;
            }
        }
        if (cp >= 32 && cp != 0x7f) {
            WindowChar(win, cp, ctrl, alt);
        }
    }
}

// ─── drawing ──────────────────────────────────────────────────────────────

static void Redraw(Window* win) {
    PlatWindow* pw = win->plat;
    if (!pw || !pw->view) {
        return;
    }
    pw->dirty = false;
    win->maximized = [pw->window isZoomed] ? true : false;
    [pw->view display];
}

// ─── window commands ──────────────────────────────────────────────────────

void AppQuit(Window* win) {
    if (win && win->plat && win->plat->window) {
        // windowWillClose is what calls WindowClosed.
        [win->plat->window close];
    }
}

void AppInvalidate(Window* win) {
    if (win && win->plat) {
        win->plat->dirty = true;
    }
}

void AppMinimize(Window* win) {
    if (win && win->plat) {
        [win->plat->window miniaturize:nil];
    }
}

void AppToggleMaximize(Window* win) {
    if (win && win->plat) {
        [win->plat->window zoom:nil];
        win->maximized = [win->plat->window isZoomed] ? true : false;
    }
}

void AppDrag(Window* win) {
    if (!win || !win->plat) {
        return;
    }
    NSEvent* ev = [NSApp currentEvent];
    if (ev) {
        [win->plat->window performWindowDragWithEvent:ev];
    }
}

void AppSetTitle(Window* win, Str title) {
    if (!win || !win->plat || !title.s) {
        return;
    }
    NSString* s = [[NSString alloc] initWithBytes:title.s
                                           length:(NSUInteger)title.len
                                         encoding:NSUTF8StringEncoding];
    if (s) {
        [win->plat->window setTitle:s];
    }
}

void PlatSetTimer(Window* win, int ms) {
    if (!win || !win->plat) {
        return;
    }
    win->plat->nextTick = ms > 0 ? TimeNow() + ms / 1000.0 : 0;
}

void PlatSetCursor(Window* win, CursorKind kind) {
    (void)win;
    if (kind == CursorKind::IBeam) {
        [[NSCursor IBeamCursor] set];
    } else {
        [[NSCursor arrowCursor] set];
    }
}

void ClipboardSetText(Window* win, Str text) {
    (void)win;
    if (!text.s || text.len <= 0) {
        return;
    }
    NSString* s = [[NSString alloc] initWithBytes:text.s
                                           length:(NSUInteger)text.len
                                         encoding:NSUTF8StringEncoding];
    if (!s) {
        return;
    }
    NSPasteboard* pb = [NSPasteboard generalPasteboard];
    [pb clearContents];
    [pb setString:s forType:NSPasteboardTypeString];
}

// ─── app lifecycle ────────────────────────────────────────────────────────

bool PlatInit(App* app) {
    (void)app;
    @autoreleasepool {
        [NSApplication sharedApplication];
        // Regular, not accessory: the examples run straight from a terminal
        // with no bundle, and this is what gives them a Dock tile and focus.
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp finishLaunching];
        [NSApp activateIgnoringOtherApps:YES];
    }
    return true;
}

void PlatShutdown(App* app) {
    (void)app;
}

Window* WindowOpen(App* app, Str title, int dipW, int dipH, WinOpts opts) {
    Window* win = WindowAlloc(app, opts);
    if (!win) {
        return nullptr;
    }
    @autoreleasepool {
        NSWindowStyleMask style =
            NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
            NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
        if (opts.borderless) {
            style = NSWindowStyleMaskBorderless | NSWindowStyleMaskResizable |
                    NSWindowStyleMaskMiniaturizable;
        }
        NSRect frame = NSMakeRect(0, 0, dipW, dipH);
        NSWindow* window =
            [[NSWindow alloc] initWithContentRect:frame
                                        styleMask:style
                                          backing:NSBackingStoreBuffered
                                            defer:NO];
        if (!window) {
            return nullptr;
        }
        GpuiView* view = [[GpuiView alloc] initWithFrame:frame];
        view->win = win;
        GpuiWindowDelegate* del = [[GpuiWindowDelegate alloc] init];
        del->win = win;

        auto* pw = new PlatWindow();
        pw->window = window;
        pw->view = view;
        pw->delegate = del;
        win->plat = pw;

        [window setContentView:view];
        [window makeFirstResponder:view];
        [window setAcceptsMouseMovedEvents:YES];
        [window setReleasedWhenClosed:NO];
        [window setDelegate:del];
        [window center];

        AppSetTitle(win, title);
        [window makeKeyAndOrderFront:nil];
        PlatSetTimer(win, WindowTimerMs(win));
    }
    return win;
}

int AppRun(App* app) {
    if (!app) {
        return 1;
    }
    while (AppAnyWindowOpen(app)) {
        @autoreleasepool {
            // Drain everything queued, then draw, then block until the next
            // event or tick — the same shape as the X11 loop.
            for (;;) {
                NSEvent* ev = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                 untilDate:nil
                                                    inMode:NSDefaultRunLoopMode
                                                   dequeue:YES];
                if (!ev) {
                    break;
                }
                [NSApp sendEvent:ev];
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
            if (!anyDirty) {
                NSDate* deadline =
                    waitS <= 0 ? [NSDate distantPast]
                               : [NSDate dateWithTimeIntervalSinceNow:waitS];
                NSEvent* ev = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                 untilDate:deadline
                                                    inMode:NSDefaultRunLoopMode
                                                   dequeue:YES];
                if (ev) {
                    [NSApp sendEvent:ev];
                }
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
    }
    return app->exitCode;
}

} // namespace gpui

// The process entry point. Examples implement GpuiMain(argc, argv).
int main(int argc, char** argv) {
    return GpuiMain(argc, argv);
}
