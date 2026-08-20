/* The Cocoa window: event pump, chrome routing, timers, clipboard, and the
   process entry point. The mirror of Window_win.cpp and Window_linux.cpp;
   everything any of them decides is delegated to WindowCommon.cpp.

   Objective-C++ under ARC (-x objective-c++ -fobjc-arc). The view is flipped,
   so a frame is drawn in points with the origin at the top left. One point is
   one DIP, which on a Retina display means the backing store is 2x and the
   drawing comes out crisp for free. */

#include "gpui/platform.h"
#include "gpui/paint.h"

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

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
    // Whether the window's class has been taught accessibilityHitTest:. Rust
    // installs it once, when the Root view is created; a Root here is built
    // every frame, so the window remembers instead.
    bool a11yInstalled = false;
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
bool WindowMacKeyDown(Window* win, NSEvent* event);
void WindowMacKeyUp(Window* win, NSEvent* event);

} // namespace gpui

// ─── the view ─────────────────────────────────────────────────────────────

namespace gpui {

// GPUI's Modifiers, out of an NSEvent's flags. `platform` is Command, and
// macOS is the one platform that reports Fn.
static Modifiers ModsOf(NSEvent* event) {
    NSEventModifierFlags f = [event modifierFlags];
    Modifiers m;
    m.control = (f & NSEventModifierFlagControl) != 0;
    m.alt = (f & NSEventModifierFlagOption) != 0;
    m.shift = (f & NSEventModifierFlagShift) != 0;
    m.platform = (f & NSEventModifierFlagCommand) != 0;
    m.function = (f & NSEventModifierFlagFunction) != 0;
    return m;
}

// An NSEvent buttonNumber as a MouseButton.
static MouseButton MouseButtonOf(NSInteger number) {
    switch (number) {
        case 0:
            return MouseButton::Left;
        case 1:
            return MouseButton::Right;
        case 2:
            return MouseButton::Middle;
        case 3:
            return MouseButton::NavigateBack;
        default:
            return MouseButton::NavigateForward;
    }
}

// Rust's Option<MouseButton> on a move: the first button currently held.
static bool PressedButton(MouseButton* out) {
    NSUInteger mask = [NSEvent pressedMouseButtons];
    for (NSInteger i = 0; i < 5; i++) {
        if (mask & (1u << i)) {
            *out = MouseButtonOf(i);
            return true;
        }
    }
    return false;
}

} // namespace gpui

@interface GpuiView : NSView <NSTextInputClient> {
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
- (BOOL)mouseDownCanMoveWindow {
    // Client title bars decide which pixels drag. Without this, AppKit also
    // moves a full-size-content window when a menu or tool button is dragged.
    return NO;
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
    gpui::MouseButton held = gpui::MouseButton::Left;
    bool any = gpui::PressedButton(&held);
    gpui::PlatformInput in = gpui::InputMouseMove((float)p.x, (float)p.y, any,
                                                  held, gpui::ModsOf(event));
    gpui::WindowDispatchInput(win, &in);
}
- (void)mouseDragged:(NSEvent*)event {
    [self mouseMoved:event];
}
- (void)rightMouseDragged:(NSEvent*)event {
    [self mouseMoved:event];
}
- (void)otherMouseDragged:(NSEvent*)event {
    [self mouseMoved:event];
}
- (void)mouseExited:(NSEvent*)event {
    NSPoint p = [self gpuiPoint:event];
    gpui::MouseButton held = gpui::MouseButton::Left;
    bool any = gpui::PressedButton(&held);
    gpui::PlatformInput in = gpui::InputMouseExited((float)p.x, (float)p.y, any,
                                                    held, gpui::ModsOf(event));
    gpui::WindowDispatchInput(win, &in);
}

- (void)mouseDown:(NSEvent*)event {
    NSPoint p = [self gpuiPoint:event];
    float x = (float)p.x;
    float y = (float)p.y;
    // Counted before the chrome is claimed: the caption drags on the first
    // press and zooms on the second, so the chrome needs the answer.
    int clicks = gpui::WindowClickCount(win, x, y, gpui::MouseButton::Left);
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
        if (clicks == 2) {
            gpui::AppToggleMaximize(win);
            return;
        }
        [[self window] performWindowDragWithEvent:event];
        return;
    }
    // first_mouse would be the press that activated the window; a Cocoa view
    // does not accept the first mouse, so that press never reaches here.
    gpui::PlatformInput in = gpui::InputMouseDown(
        gpui::MouseButton::Left, x, y, gpui::ModsOf(event), clicks, false);
    gpui::WindowDispatchInput(win, &in);
}

- (void)press:(NSEvent*)event button:(gpui::MouseButton)button {
    NSPoint p = [self gpuiPoint:event];
    float x = (float)p.x;
    float y = (float)p.y;
    gpui::PlatformInput in =
        gpui::InputMouseDown(button, x, y, gpui::ModsOf(event),
                             gpui::WindowClickCount(win, x, y, button), false);
    gpui::WindowDispatchInput(win, &in);
}

- (void)release:(NSEvent*)event button:(gpui::MouseButton)button {
    NSPoint p = [self gpuiPoint:event];
    gpui::PlatformInput in =
        gpui::InputMouseUp(button, (float)p.x, (float)p.y, gpui::ModsOf(event),
                           gpui::WindowCurrentClickCount(win));
    gpui::WindowDispatchInput(win, &in);
}

- (void)mouseUp:(NSEvent*)event {
    [self release:event button:gpui::MouseButton::Left];
}

- (void)rightMouseDown:(NSEvent*)event {
    [self press:event button:gpui::MouseButton::Right];
}

- (void)rightMouseUp:(NSEvent*)event {
    [self release:event button:gpui::MouseButton::Right];
}

// Everything that is not the left or right button: 2 is the middle one, 3 and
// 4 are the thumb buttons GPUI calls MouseButton::Navigate.
- (void)otherMouseDown:(NSEvent*)event {
    [self press:event button:gpui::MouseButtonOf([event buttonNumber])];
}

- (void)otherMouseUp:(NSEvent*)event {
    [self release:event button:gpui::MouseButtonOf([event buttonNumber])];
}

- (void)scrollWheel:(NSEvent*)event {
    NSPoint p = [self gpuiPoint:event];
    // A line of scroll is 48 DIPs, the step the other two windows use; a
    // precise (trackpad) delta is already in points, which is GPUI's
    // ScrollDelta::Pixels.
    bool precise = [event hasPreciseScrollingDeltas];
    float scale = precise ? 1.f : 48.f;
    float dx = (float)[event scrollingDeltaX] * scale;
    float dy = (float)[event scrollingDeltaY] * scale;
    gpui::TouchPhase phase = gpui::TouchPhase::Moved;
    NSEventPhase ph = [event phase];
    if (ph & NSEventPhaseBegan) {
        phase = gpui::TouchPhase::Started;
    } else if (ph & NSEventPhaseEnded) {
        phase = gpui::TouchPhase::Ended;
    } else if (ph & NSEventPhaseCancelled) {
        phase = gpui::TouchPhase::Cancelled;
    }
    gpui::PlatformInput in = gpui::InputScrollWheel(
        (float)p.x, (float)p.y, dx, dy, precise, gpui::ModsOf(event), phase);
    gpui::WindowDispatchInput(win, &in);
}

- (void)keyDown:(NSEvent*)event {
    // The keys are the window's, as they have always been. The *text* half
    // goes to the input method when a field has the keyboard: that is what
    // makes marked text possible, and it is what GPUI's own view does.
    if (gpui::WindowMacKeyDown(win, event)) {
        [self interpretKeyEvents:@[ event ]];
    }
}

// --- NSTextInputClient ---------------------------------------------------
//
// The focused InputState is the document, the way GPUI answers these against
// the window's focused EntityInputHandler. Cocoa counts in UTF-16 and the
// field counts in bytes, so every range crosses through Utf16 helpers.

- (gpui::InputState*)gpuiInput {
    gpui::InputState* in = win ? win->input : nullptr;
    return (in && in->focused) ? in : nullptr;
}

- (BOOL)hasMarkedText {
    gpui::InputState* in = [self gpuiInput];
    return (in && gpui::InputMarkedRange(in, nullptr)) ? YES : NO;
}

- (NSRange)markedRange {
    gpui::InputState* in = [self gpuiInput];
    gpui::Selection m = {};
    if (!in || !gpui::InputMarkedRange(in, &m)) {
        return NSMakeRange(NSNotFound, 0);
    }
    gpui::Str text = gpui::InputValue(in);
    NSUInteger lo = (NSUInteger)gpui::Utf8OffsetToUtf16(text, m.start);
    NSUInteger hi = (NSUInteger)gpui::Utf8OffsetToUtf16(text, m.end);
    return NSMakeRange(lo, hi - lo);
}

- (NSRange)selectedRange {
    gpui::InputState* in = [self gpuiInput];
    if (!in) {
        return NSMakeRange(NSNotFound, 0);
    }
    gpui::Str text = gpui::InputValue(in);
    NSUInteger lo =
        (NSUInteger)gpui::Utf8OffsetToUtf16(text, in->selectedRange.start);
    NSUInteger hi =
        (NSUInteger)gpui::Utf8OffsetToUtf16(text, in->selectedRange.end);
    return NSMakeRange(lo, hi - lo);
}

- (void)setMarkedText:(id)string
        selectedRange:(NSRange)selected
     replacementRange:(NSRange)replacement {
    gpui::InputState* in = [self gpuiInput];
    if (!in) {
        return;
    }
    NSString* str = [string isKindOfClass:[NSAttributedString class]]
                        ? [(NSAttributedString*)string string]
                        : (NSString*)string;
    const char* utf8 = [str UTF8String];
    gpui::Str text = utf8 ? gpui::Str(utf8, (int)strlen(utf8)) : gpui::Str{};
    gpui::Str doc = gpui::InputValue(in);
    gpui::Selection range = {};
    bool hasRange = replacement.location != NSNotFound;
    if (hasRange) {
        range.start = gpui::Utf16OffsetToUtf8(doc, (int)replacement.location);
        range.end = gpui::Utf16OffsetToUtf8(
            doc, (int)(replacement.location + replacement.length));
    }
    // The selection Cocoa reports is inside the new text, in UTF-16 units.
    gpui::Selection sel = {};
    bool hasSel = selected.location != NSNotFound;
    if (hasSel) {
        sel.start = gpui::Utf16OffsetToUtf8(text, (int)selected.location);
        sel.end = gpui::Utf16OffsetToUtf8(
            text, (int)(selected.location + selected.length));
    }
    gpui::InputReplaceAndMarkText(in, win->app, win,
                                  hasRange ? &range : nullptr, text,
                                  hasSel ? &sel : nullptr);
    gpui::AppInvalidate(win);
}

- (void)unmarkText {
    gpui::InputState* in = [self gpuiInput];
    if (in) {
        gpui::InputUnmarkText(in, win->app, win);
        gpui::AppInvalidate(win);
    }
}

- (void)insertText:(id)string replacementRange:(NSRange)replacement {
    gpui::InputState* in = [self gpuiInput];
    if (!in) {
        return;
    }
    NSString* str = [string isKindOfClass:[NSAttributedString class]]
                        ? [(NSAttributedString*)string string]
                        : (NSString*)string;
    const char* utf8 = [str UTF8String];
    if (!utf8 || !*utf8) {
        // An empty commit ends the composition without leaving anything.
        gpui::InputReplaceAndMarkText(in, win->app, win, nullptr, gpui::Str{},
                                      nullptr);
        gpui::AppInvalidate(win);
        return;
    }
    gpui::Str doc = gpui::InputValue(in);
    gpui::Selection range = {};
    bool hasRange = replacement.location != NSNotFound;
    if (hasRange) {
        range.start = gpui::Utf16OffsetToUtf8(doc, (int)replacement.location);
        range.end = gpui::Utf16OffsetToUtf8(
            doc, (int)(replacement.location + replacement.length));
    }
    // A null range is the marked run, which is what a commit replaces.
    gpui::InputReplaceTextInRange(in, win->app, win,
                                  hasRange ? &range : nullptr,
                                  gpui::Str(utf8, (int)strlen(utf8)));
    gpui::AppInvalidate(win);
}

- (void)doCommandBySelector:(SEL)selector {
    // Every command key — Return, Tab, the arrows, Backspace — has already
    // gone through WindowKeyDown, which is where this port binds them. Taking
    // them again here would run them twice, and letting NSView have them
    // would make Cocoa beep.
    (void)selector;
}

- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range
                                               actualRange:
                                                   (NSRangePointer)actual {
    gpui::InputState* in = [self gpuiInput];
    if (!in || range.location == NSNotFound) {
        return nil;
    }
    gpui::Str doc = gpui::InputValue(in);
    int lo = gpui::Utf16OffsetToUtf8(doc, (int)range.location);
    int hi = gpui::Utf16OffsetToUtf8(doc, (int)(range.location + range.length));
    if (lo < 0 || hi > doc.len || hi < lo) {
        return nil;
    }
    if (actual) {
        *actual = range;
    }
    NSString* str = [[NSString alloc] initWithBytes:doc.s + lo
                                             length:(NSUInteger)(hi - lo)
                                           encoding:NSUTF8StringEncoding];
    return str ? [[NSAttributedString alloc] initWithString:str] : nil;
}

- (NSArray<NSAttributedStringKey>*)validAttributesForMarkedText {
    return @[];
}

- (NSRect)firstRectForCharacterRange:(NSRange)range
                         actualRange:(NSRangePointer)actual {
    (void)range;
    if (actual) {
        *actual = range;
    }
    gpui::InputState* in = [self gpuiInput];
    if (!in) {
        return NSZeroRect;
    }
    // Where the candidate list hangs from: the caret, in screen space.
    float x = in->lastBounds.x + in->caretX - in->scrollX;
    float y = in->lastBounds.y - in->scrollY;
    NSRect local = NSMakeRect(x, y, 1, in->lastLineH);
    NSRect inWindow = [self convertRect:local toView:nil];
    return [[self window] convertRectToScreen:inWindow];
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point {
    (void)point;
    return NSNotFound;
}

- (void)keyUp:(NSEvent*)event {
    gpui::WindowMacKeyUp(win, event);
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

- (void)windowDidBecomeKey:(NSNotification*)note {
    (void)note;
    gpui::WindowSetActive(win, true);
}

- (void)windowDidResignKey:(NSNotification*)note {
    (void)note;
    gpui::WindowSetActive(win, false);
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

// The release of a key. Only the activation keys do anything with one, and
// they need no text, so this is the modifier and code half of the press.
void WindowMacKeyUp(Window* win, NSEvent* event) {
    if (!win) {
        return;
    }
    NSEventModifierFlags mods = [event modifierFlags];
    NSString* bare = [event charactersIgnoringModifiers];
    unichar first = [bare length] > 0 ? [bare characterAtIndex:0] : 0;
    int key = KeyFor(first);
    if (!key) {
        return;
    }
    WindowKeyUp(
        win, key, (mods & NSEventModifierFlagShift) != 0,
        (mods & (NSEventModifierFlagControl | NSEventModifierFlagCommand)) != 0,
        (mods & NSEventModifierFlagOption) != 0);
}

// True when the caller should hand the event to the input method for its
// text: a field has the keyboard and this keystroke was not a chord the
// window took for itself.
bool WindowMacKeyDown(Window* win, NSEvent* event) {
    if (!win) {
        return false;
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
    // Backspace arrives as a key only; the bound InputState edits on the
    // control code the Windows window delivers through WM_CHAR.
    if (key == KeyBack) {
        WindowChar(win, 8, ctrl, alt);
        return false;
    }
    if (ctrl || alt || key == KeyReturn || key == KeyTab || key == KeyEscape) {
        return false;
    }
    // A focused field's text is the input method's to deliver, through
    // insertText: and setMarkedText:. Everywhere else the codepoints go
    // straight through, which is what the window's own key listener reads.
    if (win->input && win->input->focused) {
        return true;
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
    return false;
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
    } else if (kind == CursorKind::Pointer) {
        [[NSCursor pointingHandCursor] set];
    } else if (kind == CursorKind::ColResize) {
        [[NSCursor resizeLeftRightCursor] set];
    } else if (kind == CursorKind::RowResize) {
        [[NSCursor resizeUpDownCursor] set];
    } else {
        [[NSCursor arrowCursor] set];
    }
}

int PlatDoubleClickMs() {
    // NSEvent tracks its own clickCount, but the window counts presses in
    // shared code so all three platforms agree on what a run is; this is the
    // one number the OS owns.
    return (int)([NSEvent doubleClickInterval] * 1000);
}

// macos_accessibility.rs hit_test_forwarder: the window hands the point to
// its content view, which is the one thing in the tree that knows what was
// drawn where. class_addMethod leaves an existing implementation alone, so a
// window that already answers keeps its own.
static id GpuiAccessibilityHitTest(id self, SEL cmd, NSPoint point) {
    (void)cmd;
    NSView* view = [(NSWindow*)self contentView];
    if (!view) {
        return nil;
    }
    return [view accessibilityHitTest:point];
}

void PlatInstallAccessibilityHitTest(Window* win) {
    if (!win || !win->plat || !win->plat->window) {
        return;
    }
    if (win->plat->a11yInstalled) {
        return;
    }
    win->plat->a11yInstalled = true;
    Class cls = object_getClass(win->plat->window);
    if (!cls) {
        return;
    }
    // "@@:{CGPoint=dd}": returns an object, takes self, the selector and a
    // point of two doubles — the encoding Rust builds from the same pieces.
    class_addMethod(cls, @selector(accessibilityHitTest:),
                    (IMP)GpuiAccessibilityHitTest, "@@:{CGPoint=dd}");
}

bool PlatHasMenu() {
    return true;
}

// Which row was chosen. popUpMenuPositioningItem runs its own tracking loop
// and returns once the menu is gone, so one variable is enough to carry the
// answer back out of the action.
static int gMenuChoice = 0;

} // namespace gpui

// The object every row of a native menu sends its action to.
@interface GpuiMenuTarget : NSObject
@end

@implementation GpuiMenuTarget
- (void)gpuiMenuItemChosen:(id)sender {
    gpui::gMenuChoice = (int)[(NSMenuItem*)sender tag];
}
@end

namespace gpui {

// The rows as an NSMenu. A row's tag is the id it reports; a submenu row
// carries no tag and opens onto a menu built the same way.
// MENU_IMAGE_SIZE: the side a menu item's image is scaled to, in points.
static const int kMenuImageSize = 16;

// One icon as an image the menu can show. It is loaded as a template image,
// which is what makes AppKit tint it with the row's text — so it reads right
// in either appearance and while the row is highlighted.
static NSImage* MenuIconImage(Window* win, const char* path) {
    if (!win || !path || !path[0]) {
        return nil;
    }
    // Rasterized at twice the point size, so it stays sharp on a Retina
    // display; the image is then told it measures the point size.
    const int px = kMenuImageSize * 2;
    Vec<uint8_t> buf;
    buf.AppendBlanks(px * px * 4);
    // The colour does not matter for a template image, only the coverage.
    if (!SvgRasterize(win->paint.pa, Str(path), px, Rgb(0, 0, 0), buf.els)) {
        return nil;
    }
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGBitmapInfo bitmapInfo =
        (CGBitmapInfo)((uint32_t)kCGImageAlphaPremultipliedFirst |
                       (uint32_t)kCGBitmapByteOrder32Little);
    CGContextRef bmp = CGBitmapContextCreate(buf.els, (size_t)px, (size_t)px, 8,
                                             (size_t)px * 4, space, bitmapInfo);
    CGColorSpaceRelease(space);
    if (!bmp) {
        return nil;
    }
    CGImageRef cgImage = CGBitmapContextCreateImage(bmp);
    CGContextRelease(bmp);
    if (!cgImage) {
        return nil;
    }
    NSImage* image = [[NSImage alloc]
        initWithCGImage:cgImage
                   size:NSMakeSize(kMenuImageSize, kMenuImageSize)];
    CGImageRelease(cgImage);
    [image setTemplate:YES];
    return image;
}

static NSMenu* BuildMenu(Window* win, const PlatMenuItem* items, int n,
                         GpuiMenuTarget* target) {
    NSMenu* menu = [[NSMenu alloc] init];
    // Without this AppKit greys every row whose target does not answer
    // validateMenuItem:, which is all of them.
    [menu setAutoenablesItems:NO];
    for (int i = 0; i < n; i++) {
        const PlatMenuItem& it = items[i];
        if (it.separator) {
            [menu addItem:[NSMenuItem separatorItem]];
            continue;
        }
        NSString* label =
            [NSString stringWithUTF8String:(it.label ? it.label : "")];
        NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:label
                                                      action:nil
                                               keyEquivalent:@""];
        [item setEnabled:(it.disabled ? NO : YES)];
        if (it.iconPath) {
            NSImage* image = MenuIconImage(win, it.iconPath);
            if (image) {
                [item setImage:image];
            }
        }
        if (it.submenu && it.submenuN > 0) {
            [item setSubmenu:BuildMenu(win, it.submenu, it.submenuN, target)];
        } else {
            [item setTag:it.id];
            [item setState:(it.checked ? NSControlStateValueOn
                                       : NSControlStateValueOff)];
            if (!it.disabled) {
                [item setTarget:target];
                [item setAction:@selector(gpuiMenuItemChosen:)];
            }
        }
        [menu addItem:item];
    }
    return menu;
}

int PlatShowMenu(Window* win, const PlatMenuItem* items, int n, float x,
                 float y, bool dark) {
    (void)dark;
    if (!win || !win->plat || !items || n <= 0) {
        return 0;
    }
    GpuiMenuTarget* target = [[GpuiMenuTarget alloc] init];
    NSMenu* menu = BuildMenu(win, items, n, target);
    gMenuChoice = 0;
    // The view is flipped, so the position is the one the window works in.
    NSPoint at = NSMakePoint(x, y);
    [menu popUpMenuPositioningItem:nil atLocation:at inView:win->plat->view];
    return gMenuChoice;
}

// cx.open_url. NSWorkspace hands the URL to whichever application is
// registered for its scheme.
// The macOS switch is Accessibility ▸ Display ▸ Reduce motion, which
// NSWorkspace answers for directly.
bool PlatReduceMotion() {
    return
        [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldReduceMotion]
            ? true
            : false;
}

void OpenUrl(Str url) {
    if (!url.s || url.len <= 0) {
        return;
    }
    NSString* s = [[NSString alloc] initWithBytes:url.s
                                           length:(NSUInteger)url.len
                                         encoding:NSUTF8StringEncoding];
    NSURL* u = s ? [NSURL URLWithString:s] : nil;
    if (u) {
        [[NSWorkspace sharedWorkspace] openURL:u];
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

Str ClipboardGetText(Arena* a, Window* win) {
    (void)win;
    NSPasteboard* pb = [NSPasteboard generalPasteboard];
    NSString* s = [pb stringForType:NSPasteboardTypeString];
    if (!s) {
        return {};
    }
    const char* utf8 = [s UTF8String];
    if (!utf8) {
        return {};
    }
    int n = (int)strlen(utf8);
    char* buf = (char*)Alloc(a, n + 1);
    if (!buf) {
        return {};
    }
    memcpy(buf, utf8, (size_t)n + 1);
    return Str(buf, n);
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
        if (opts.clientTitleBar) {
            style |= NSWindowStyleMaskFullSizeContentView;
        }
        if (opts.borderless) {
            style = NSWindowStyleMaskBorderless | NSWindowStyleMaskResizable |
                    NSWindowStyleMaskMiniaturizable;
        }
        // GPUI's primary_display().bounds() is the full display, not the
        // work area below the menu bar and above the Dock.
        NSRect screen = [[NSScreen mainScreen] frame];
        WindowClampToDisplay(&dipW, &dipH, (int)screen.size.width,
                             (int)screen.size.height);
        NSRect frame = NSMakeRect(0, 0, dipW, dipH);
        NSWindow* window =
            [[NSWindow alloc] initWithContentRect:frame
                                        styleMask:style
                                          backing:NSBackingStoreBuffered
                                            defer:NO];
        if (!window) {
            return nullptr;
        }
        if (opts.clientTitleBar) {
            [window setTitleVisibility:NSWindowTitleHidden];
            [window setTitlebarAppearsTransparent:YES];
            [window setTitlebarSeparatorStyle:NSTitlebarSeparatorStyleNone];
            [window setMovableByWindowBackground:NO];
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
        // AppNew may run well before a window is opened (system_monitor does
        // an initial metrics sweep). Activate again now that Cocoa has a key
        // window to bring forward.
        [NSApp activateIgnoringOtherApps:YES];
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
    // Strip the -gpui-* flags here too, so an example parses the same argv on
    // every platform. -gpui-window itself is only honoured on Windows, where
    // the screenshot harness runs.
    argc = gpui::GpuiTakeRuntimeArgs(argc, argv);
    return GpuiMain(argc, argv);
}
