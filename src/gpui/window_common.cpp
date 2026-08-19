/* Everything a window does that is not the OS window: frame drawing, input
   dispatch, the app lifecycle. Window_win.cpp and Window_linux.cpp call in
   here; nothing here calls back out except through Platform.h. */

#include "gpui/platform.h"
#include "gpui/paint.h"

namespace gpui {

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

// ─── frame ────────────────────────────────────────────────────────────────

void WindowDrawFrame(Window* win, void* native, int pxW, int pxH, float dipW,
                     float dipH) {
    if (!win) {
        return;
    }
    double drawStart = TimeNow();
    if (!PaintTargetBegin(&win->paint, native, pxW, pxH)) {
        return;
    }

    if (win->frameArena) {
        win->frameArena->Reset();
    } else {
        win->frameArena = ArenaNew();
    }
    ResetTempArena();
    win->paint.hits.Clear();
    win->paint.scrolls.Clear();
    win->paint.texts.Clear();
    win->paint.inputs.Clear();
    win->paint.textDocLen = 0;
    win->paint.selA = -1;
    win->paint.selB = -1;
    win->paint.hoverId = win->hoverId;
    win->paint.focusId = win->focusId;
    win->paint.viewW = dipW;
    win->paint.viewH = dipH;
    TextMeasBeginFrame(&win->paint);

    // Whatever the view pointed win->input at is the focused field. Start its
    // caret and stop the one that lost focus, so no app has to. Rust hangs
    // this off InputState::on_focus / on_blur, which is where InputFocus does
    // it too; this is the same handoff for a view that points win->input at a
    // field itself rather than calling InputFocus.
    if (win->input != win->prevInput) {
        if (win->prevInput) {
            BlinkStop(win->app, win, &win->prevInput->blink);
        }
        if (win->input) {
            BlinkStart(win->app, win, &win->input->blink);
        }
        win->prevInput = win->input;
    }

    El* root = EntityRender(win->app, win, win->frameArena, win->root);

    const Theme& th = ThemeNow();
    CanvasClear(&win->paint, th.background);
    if (root) {
        LayoutEl(&win->paint, root, 0, 0, dipW, dipH, 16.f, th.foreground);
        FocusCollect(win, root);
        PaintEl(&win->paint, root);
    }
    // Rust renders TooltipOverlay deferred with priority 2, so the tip is over
    // everything the frame drew. It is the overlay's, not the trigger's: by
    // the time the show countdown lands, the frame that asked for it is gone.
    TooltipPaint(&win->paint, TooltipShowing(win));

    PaintTargetEnd(&win->paint);
    TextMeasEndFrame(&win->paint);

    // Record the frame for the trace. GPUI times Window::draw, which is this
    // whole function: build the element tree, lay it out, paint it.
    FrameTiming timing;
    timing.drawSecs = (float)(TimeNow() - drawStart);
    win->frameTrace[win->frameSeq % (uint64_t)kFrameTraceCap] = timing;
    win->frameSeq++;
}

// The hit rect an element id painted, from the last frame. The tree is
// Whether the frame gave this id a focus handle. CollectFocus walks the tree
// for `FocusId`, so an element that only has `Click(id)` is missing here.
static bool FocusIdIsFocusable(Window* win, int id) {
    for (int i = 0; i < win->focusEls.len; i++) {
        if (win->focusEls[i].id == id) {
            return true;
        }
    }
    return false;
}

// rebuilt every frame, so an id is the only handle that survives one.
static const HitRect* HitRectById(Window* win, int id) {
    if (!win || !id) {
        return nullptr;
    }
    for (int i = win->paint.hits.len - 1; i >= 0; i--) {
        if (win->paint.hits[i].id == id) {
            return &win->paint.hits[i];
        }
    }
    return nullptr;
}

// ─── input ────────────────────────────────────────────────────────────────

void WindowKeyDown(Window* win, int key, bool shift, bool ctrl, bool alt) {
    if (!win) {
        return;
    }
    if (key == KeyTab) {
        int trap = 0;
        for (int i = 0; i < win->focusEls.len; i++) {
            if (win->focusEls[i].id == win->focusId) {
                trap = win->focusEls[i].trapId;
                break;
            }
        }
        FocusNext(win, trap, shift);
        AppInvalidate(win);
        return;
    }
    // The focused field gets the chord first, as GPUI dispatches an action to
    // whatever has focus before anything else sees the key. The view's own
    // subscription still hears it — that is Rust's cx.propagate(), which every
    // action the input does not consume ends with — but a key the field ate is
    // not also an Enter on the focused element.
    bool eaten = false;
    if (win->input && win->input->focused) {
        InputAction action =
            InputActionForKey(win->input, key, shift, ctrl, alt);
        eaten = InputPerform(win->input, win->app, win, action, shift);
    }
    if (win->onKey.IsValid()) {
        KeyEvent ev = {};
        ev.vk = key;
        ev.down = true;
        ev.shift = shift;
        ev.ctrl = ctrl;
        ev.alt = alt;
        ListenerCall(win->app, win, win->onKey, &ev);
    }
    // Enter and Space both activate the focused element: run that element's
    // own listener, the one a click on it would have run. GPUI turns either
    // keystroke on a focused clickable element into a click, which is what
    // checkbox.rs's `enter_and_space_each_emit_once` pins. A focused field
    // takes the space as text instead, so it never reaches the element.
    bool activates = (key == KeyReturn && !win->eatReturn) ||
                     (key == KeySpace && !(win->input && win->input->focused));
    if (activates && win->focusId && !eaten) {
        const HitRect* focused = HitRectById(win, win->focusId);
        // GPUI's ClickEvent::Keyboard: no pointer was involved, so the
        // position is the element's own box.
        ClickEvent ev = {0, 0, MouseButton::Left, win->focusId};
        ev.keyboard = true;
        if (focused) {
            ev.x = focused->bounds.CenterX();
            ev.y = focused->bounds.CenterY();
            ev.el = focused->bounds;
        }
        if (focused && focused->listener.IsValid()) {
            ListenerCall(win->app, win, focused->listener, &ev);
        } else if (win->onClick.IsValid()) {
            ListenerCall(win->app, win, win->onClick, &ev);
        }
    }
    win->eatReturn = false;
    AppInvalidate(win);
}

void WindowChar(Window* win, uint32_t ch, bool ctrl, bool alt) {
    if (!win) {
        return;
    }
    if (win->onKey.IsValid() && ch >= 32) {
        KeyEvent ev = {};
        ev.ch = ch;
        ev.down = true;
        ev.ctrl = ctrl;
        ev.alt = alt;
        ListenerCall(win->app, win, win->onKey, &ev);
    }
    // A typed character reaches the focused field the way GPUI hands one to
    // the focused EntityInputHandler. The control codes are keys, not text:
    // backspace, tab, return and escape all came through WindowKeyDown
    // already, and Ctrl+letter arrives here as 1..26.
    if (win->input && win->input->focused && ch >= 32 && ch != 127 && !ctrl &&
        !alt) {
        InputTypeChar(win->input, win->app, win, ch);
    }
    AppInvalidate(win);
}

// cx.emit(SliderEvent::..) — the subscription lives on the state, the way
// InputState::onChange does.
static void SliderEmit(Window* win, SliderState* s, SliderEventKind kind) {
    if (!s->onChange.IsValid()) {
        return;
    }
    SliderEvent ev = {kind, s->value};
    ListenerCall(win->app, win, s->onChange, &ev);
}

// SliderTrack::on_mouse_down and its on_drag_move: a press jumps the value to
// where it landed and takes the nearer end of a range; every move until the
// release keeps that end following the pointer.
static void SliderPress(Window* win, const HitRect* hit, Point at) {
    SliderState* s = hit->slider;
    // The rail reported its own box when it painted; a slider built without
    // one maps against the box that took the press instead.
    if (s->bounds.w <= 0 || s->bounds.h <= 0) {
        SliderSetBounds(s, hit->bounds);
    }
    s->dragStart = SliderIsStartAt(s, hit->sliderAxis, at);
    if (SliderUpdateByPosition(s, hit->sliderAxis, at, s->dragStart)) {
        SliderEmit(win, s, SliderEventKind::Change);
    }
    AppInvalidate(win);
}

// InputState::on_mouse_down. A press focuses the field, puts the caret where
// it landed and opens a drag; shift extends the selection instead of dropping
// it, a second press takes the word and a third the line. A press anywhere
// else blurs whatever had focus, which is what GPUI's focus handle does.
static void InputPress(Window* win, const MouseDownEvent& in) {
    InputState* s = InputAtPosition(&win->paint, in.x, in.y);
    if (!s) {
        if (win->input) {
            InputBlur(win->input, win->app, win);
        }
        return;
    }
    if (s->disabled) {
        return;
    }
    if (!s->focused) {
        InputFocus(s, win->app, win);
    }
    int offset = InputIndexForPosition(s, &win->paint, in.x, in.y);
    if (in.clickCount >= 3) {
        InputSelectLine(s, win->app, win, offset);
    } else if (in.clickCount == 2) {
        InputSelectWord(s, win->app, win, offset);
    } else if (in.modifiers.shift) {
        InputSelectTo(s, win->app, win, offset);
    } else {
        InputMoveTo(s, win->app, win, offset);
    }
    s->selecting = true;
}

// ─── scrollbar ────────────────────────────────────────────────────────────
//
// crates/base/src/scrollbar.rs installs a MouseDownEvent handler over the
// bar's bounds and a MouseMoveEvent one for the drag. The arithmetic is in
// src/base/scrollbar.cpp; this is the routing.

// The scrolled box whose scrollbar band the pointer is in, or null. Innermost
// first, the way the hit test reads its rects.
static const ScrollRect* ScrollbarAt(PaintCtx* ctx, float x, float y) {
    for (int i = ctx->scrolls.len - 1; i >= 0; i--) {
        const ScrollRect& s = ctx->scrolls[i];
        if (!s.onScroll.IsValid() || s.contentH <= s.bounds.h + 1.f) {
            continue;
        }
        if (y < s.bounds.y || y > s.bounds.Bottom()) {
            continue;
        }
        if (x >= s.bounds.Right() - kScrollbarBandW && x <= s.bounds.Right()) {
            return &ctx->scrolls[i];
        }
    }
    return nullptr;
}

static void ScrollbarEmit(Window* win, const ScrollRect* s, float offsetY) {
    ScrollEvent ev = {s->id, offsetY};
    ListenerCall(win->app, win, s->onScroll, &ev);
    AppInvalidate(win);
}

// The press. Inside the thumb it opens a drag and keeps where it landed;
// anywhere else on the track the thumb jumps its centre to the press, which
// is Rust's two branches on `thumb_bounds.contains`.
static void ScrollbarPress(Window* win, const ScrollRect* s, float y) {
    float thumb = ScrollbarThumbSize(s->bounds.h, s->bounds.h, s->contentH);
    float thumbTop =
        s->bounds.y + ScrollbarThumbPos(s->bounds.h, thumb, s->scrollY,
                                        s->bounds.h, s->contentH);
    win->scrollDragId = s->id;
    if (y >= thumbTop && y <= thumbTop + thumb) {
        win->scrollDragGrab = y - thumbTop;
        return;
    }
    // A track press grabs the thumb by its middle, so the drag that may
    // follow carries on from where it just landed.
    win->scrollDragGrab = thumb * 0.5f;
    ScrollbarEmit(
        win, s,
        ScrollbarOffsetForTrackPress(y, s->bounds.y, s->bounds.h, thumb,
                                     s->bounds.h, s->contentH));
}

// The scroll rect of an id, from the frame on screen.
static const ScrollRect* ScrollRectById(Window* win, int id) {
    for (int i = win->paint.scrolls.len - 1; i >= 0; i--) {
        if (win->paint.scrolls[i].id == id) {
            return &win->paint.scrolls[i];
        }
    }
    return nullptr;
}

static void ScrollbarDrag(Window* win, float y) {
    const ScrollRect* s = ScrollRectById(win, win->scrollDragId);
    if (!s || !s->onScroll.IsValid()) {
        return;
    }
    float thumb = ScrollbarThumbSize(s->bounds.h, s->bounds.h, s->contentH);
    ScrollbarEmit(
        win, s,
        ScrollbarOffsetForDrag(y, win->scrollDragGrab, s->bounds.y, s->bounds.h,
                               thumb, s->bounds.h, s->contentH));
}

static void SliderDrag(Window* win, const HitRect* hit, Point at) {
    SliderState* s = hit->slider;
    if (SliderUpdateByPosition(s, hit->sliderAxis, at, s->dragStart)) {
        SliderEmit(win, s, SliderEventKind::Change);
        AppInvalidate(win);
    }
}

// Slider's on_mouse_up + on_mouse_up_out, which Rust puts on the root so a
// release counts wherever it lands. Every slider the frame painted is asked;
// handle_release clears the flag, so one that was not being dragged says
// nothing and a state bound twice only answers once.
static void SliderRelease(Window* win) {
    for (int i = 0; i < win->paint.hits.len; i++) {
        SliderState* s = win->paint.hits[i].slider;
        if (s && SliderHandleRelease(s)) {
            SliderEmit(win, s, SliderEventKind::Release);
            AppInvalidate(win);
        }
    }
}

static void DispatchMouseMove(Window* win, const MouseMoveEvent& in) {
    float x = in.x;
    float y = in.y;
    win->mouseX = x;
    win->mouseY = y;
    // An I-beam over anything selectable, the way every text view does it.
    // TextHitOffsetAt only answers for text that asked to be Selectable().
    // Anything else, the element under the pointer says what shape it wants —
    // and a drag keeps the shape it started with, so a column edge stays a
    // resize cursor while the pointer runs off it.
    CursorKind want = CursorKind::Arrow;
    const HitRect* under = HitRectById(win, win->pressedId);
    if (!under || !win->mouseDown) {
        under = HitTestRect(&win->paint, x, y);
    }
    if (TextHitOffsetAt(&win->paint, x, y, false) >= 0) {
        want = CursorKind::IBeam;
    } else if (under) {
        want = under->cursor;
    }
    if (want != win->cursor) {
        win->cursor = want;
        PlatSetCursor(win, want);
    }
    int id = HitTest(&win->paint, x, y);
    if (id != win->hoverId) {
        // div().on_hover(..): the element the pointer left hears false and the
        // one it entered hears true. Both are read off the frame that is still
        // on screen, before hoverId moves, so the leaving element is still
        // findable.
        HoverEvent left = {false};
        HoverEvent entered = {true};
        const HitRect* was = HitRectById(win, win->hoverId);
        const HitRect* now = HitRectById(win, id);
        win->hoverId = id;
        if (was && was->onHover.IsValid()) {
            ListenerCall(win->app, win, was->onHover, &left);
        }
        if (now && now->onHover.IsValid()) {
            ListenerCall(win->app, win, now->onHover, &entered);
        }
        // El::Tip is a tooltip trigger. Rust's triggers call request_show and
        // request_hide on the window's one overlay; the hover change is where
        // that happens here, since the trigger is a style flag rather than an
        // element that could carry handlers of its own.
        if (now && now->tooltip.s) {
            TooltipRequestShow(win, now->tooltip, now->bounds);
        } else {
            TooltipRequestHide(win);
        }
        AppInvalidate(win);
    }
    if (win->onMouseMove.IsValid()) {
        ListenerCall(win->app, win, win->onMouseMove, &in);
    }
    // on_drag_move: the element that took the press hears every move until
    // the release, wherever the pointer has got to by then. The press picked
    // up whatever payload the element named with on_drag, and every move
    // carries it back the way DragMoveEvent<T> does.
    // drag_over: which drop target the pointer is over, so an element that
    // takes this kind of drag can show itself while one is in flight.
    if (win->activeDrag.IsValid()) {
        const HitRect* over =
            HitTestDrop(&win->paint, x, y, win->activeDrag.kind);
        win->dragOverId = over ? over->id : 0;
    }
    const HitRect* pressed = HitRectById(win, win->pressedId);
    if (pressed && pressed->onDragMove.IsValid()) {
        DragMoveEvent ev = {pressed->drag, in, pressed->bounds};
        ListenerCall(win->app, win, pressed->onDragMove, &ev);
    }
    if (pressed && pressed->slider) {
        SliderDrag(win, pressed, {x, y});
    }
    // The bar keeps every move until the release, wherever the pointer has
    // got to — the same rule the slider and on_drag_move go by.
    if (win->scrollDragId && win->mouseDown) {
        ScrollbarDrag(win, y);
    }
    // InputState::on_drag_move: the field that took the press keeps every move
    // until the release, wherever the pointer has got to. The button being
    // held is `win->mouseDown` rather than the move event's own flag, the same
    // signal the slider drag and on_drag_move above go by.
    if (win->input && win->input->selecting && win->mouseDown) {
        InputSelectTo(win->input, win->app, win,
                      InputIndexForPosition(win->input, &win->paint, x, y));
    }
    if (win->mouseDown) {
        AppInvalidate(win);
    }
}

// How far the pointer may wander between two presses and still be one run.
// Windows asks the OS with SM_CXDOUBLECLK, which is 4 px on every default
// install; the other two have no setting to ask for.
static const float kClickSlop = 4;
// The title bar is 34 tall; a double click on empty chrome maximizes.
static const float kCaptionH = 34;

int WindowClickCount(Window* win, float x, float y, MouseButton button) {
    if (!win) {
        return 1;
    }
    double now = TimeNow();
    float dx = x - win->lastDownX;
    float dy = y - win->lastDownY;
    bool sameRun = win->clickRun > 0 && button == win->lastDownButton &&
                   now - win->lastDownAt <= PlatDoubleClickMs() / 1000.0 &&
                   dx * dx + dy * dy <= kClickSlop * kClickSlop;
    win->clickRun = sameRun ? win->clickRun + 1 : 1;
    win->lastDownAt = now;
    win->lastDownX = x;
    win->lastDownY = y;
    win->lastDownButton = button;
    return win->clickRun;
}

int WindowCurrentClickCount(Window* win) {
    return win && win->clickRun > 0 ? win->clickRun : 1;
}

static void DispatchMouseDown(Window* win, const MouseDownEvent& in) {
    float x = in.x;
    float y = in.y;
    if (win->onMouseDown.IsValid()) {
        ListenerCall(win->app, win, win->onMouseDown, &in);
    }
    // Only the left button clicks an element. GPUI routes a press of any
    // button to whatever asked for that button, and never turns a right one
    // into a click; so a non-left press reaches the element's own
    // on_mouse_down — which is how a popover opens on the right button — and
    // stops before the click path, the focus move and the caret below.
    if (!in.IsFocusing()) {
        const HitRect* other = HitTestRect(&win->paint, x, y);
        if (other && other->onMouseDown.IsValid()) {
            MouseDownEvent ev = in;
            ev.el = other->bounds;
            ListenerCall(win->app, win, other->onMouseDown, &ev);
        }
        AppInvalidate(win);
        return;
    }
    // The scrollbar sits over whatever it scrolls, so it is asked first: a
    // press on the bar is the bar's, not the row underneath it. Rust says the
    // same with cx.stop_propagation().
    const ScrollRect* bar = ScrollbarAt(&win->paint, x, y);
    if (bar) {
        win->mouseDown = true;
        ScrollbarPress(win, bar, y);
        AppInvalidate(win);
        return;
    }
    const HitRect* hit = HitTestRect(&win->paint, x, y);
    int id = hit ? hit->id : 0;
    win->mouseDown = true;
    win->pressedId = id;
    // window.active_drag: a press on an element with a payload starts the
    // drag, and it lasts until the button comes back up.
    win->activeDrag = hit ? hit->drag : DragPayload{};
    win->dragOverId = 0;
    // A press takes focus only where there is a focus handle to take. Rust
    // gives a disabled widget its element id all the same — `div().id(id)` is
    // what makes it hit-testable and hoverable — and hangs `track_focus` off
    // `when(!disabled)`, so pressing one leaves focus where it was.
    if (id && FocusIdIsFocusable(win, id)) {
        win->focusId = id;
    }
    // on_mouse_down, ahead of the click: an element that wants the press
    // itself — a slider jumping to it — gets the whole event, not the
    // ClickEvent the click path builds.
    if (hit && hit->onMouseDown.IsValid()) {
        MouseDownEvent ev = in;
        ev.el = hit->bounds;
        ListenerCall(win->app, win, hit->onMouseDown, &ev);
    }
    if (hit && hit->slider) {
        SliderPress(win, hit, {x, y});
    }
    InputPress(win, in);
    ClickEvent ev = {x, y, in.button, id};
    ev.clickCount = in.clickCount;
    ev.modifiers = in.modifiers;
    if (hit) {
        ev.el = hit->bounds;
    }
    if (hit && hit->listener.IsValid()) {
        ListenerCall(win->app, win, hit->listener, &ev);
    } else if (win->onClick.IsValid() && !(hit && hit->slider)) {
        // A press on a slider is handled by the slider, so it is not the
        // outside click that dismisses an overlay.
        ListenerCall(win->app, win, win->onClick, &ev);
    }
    if (hit && hit->onClick.IsValid()) {
        hit->onClick.Call();
    }
    // TitleBar::on_double_click -> window.zoom_window(), in title_bar.rs. The
    // press was dispatched first, so an element that put itself in the title
    // bar still saw it — Rust bubbles the same way. The empty half of the band
    // counts too, since the gap between a TitleBar's controls is no hit rect
    // of its own — but only in a window that draws its own title bar: in one
    // wearing the system caption the top of the client area is ordinary
    // content, and a double click there is not a zoom. Windows answers
    // WM_NCHITTEST with HTCAPTION over the caption and never sends that press
    // here at all, so on that platform this is only the empty half.
    bool caption = id == ClickWinCaption ||
                   (id == 0 && win->opts.clientTitleBar && y < kCaptionH);
    if (in.clickCount == 2 && caption) {
        AppToggleMaximize(win);
    }
    AppInvalidate(win);
}

static void DispatchMouseUp(Window* win, const MouseUpEvent& in) {
    win->mouseDown = false;
    // with_unset_drag_pos: the release ends the scrollbar drag wherever it
    // landed.
    win->scrollDragId = 0;
    win->scrollDragGrab = 0;
    if (win->onMouseUp.IsValid()) {
        ListenerCall(win->app, win, win->onMouseUp, &in);
    }
    // The element under the pointer hears the release, then the one that took
    // the press stops being held. A drag that ended somewhere else leaves the
    // first of those empty, which is what on_mouse_up_out is for.
    const HitRect* hit = HitTestRect(&win->paint, in.x, in.y);
    if (hit && hit->onMouseUp.IsValid()) {
        ListenerCall(win->app, win, hit->onMouseUp, &in);
    }
    // on_mouse_up_out: every element that asked for the release it did not
    // get. Rust hears one wherever the pointer is, so this walks the frame
    // rather than only the element that took the press — a drag that ended
    // off the edge is the case it exists for.
    for (int i = 0; i < win->paint.hits.len; i++) {
        const HitRect& hr = win->paint.hits[i];
        if (hr.onMouseUpOut.IsValid() && !hr.bounds.Contains({in.x, in.y})) {
            ListenerCall(win->app, win, hr.onMouseUpOut, &in);
        }
    }
    // on_drop: the element under the pointer that takes this drag hears where
    // it landed. It runs after on_mouse_up_out, so a source that is winding
    // its own drag down has already done so by the time the target acts.
    if (win->activeDrag.IsValid()) {
        const HitRect* target =
            HitTestDrop(&win->paint, in.x, in.y, win->activeDrag.kind);
        if (target) {
            DropEvent ev = {win->activeDrag, in.x, in.y, target->bounds};
            ListenerCall(win->app, win, target->onDrop, &ev);
        }
        win->activeDrag = {};
        win->dragOverId = 0;
    }
    SliderRelease(win);
    // InputState::on_mouse_up: the drag is over, and the word a double click
    // pinned stops holding the selection open.
    if (win->input && win->input->selecting) {
        win->input->selecting = false;
        win->input->hasSelectedWordRange = false;
    }
    win->pressedId = 0;
    AppInvalidate(win);
}

static void DispatchMouseExited(Window* win, const MouseExitEvent& in) {
    win->hoverId = 0;
    if (win->onMouseExit.IsValid()) {
        ListenerCall(win->app, win, win->onMouseExit, &in);
    }
    AppInvalidate(win);
}

static void DispatchScrollWheel(Window* win, const ScrollWheelEvent& in) {
    if (win->onScrollWheel.IsValid()) {
        ListenerCall(win->app, win, win->onScrollWheel, &in);
    }
    AppInvalidate(win);
}

void WindowDispatchInput(Window* win, const PlatformInput* input) {
    if (!win || !input) {
        return;
    }
    switch (input->kind) {
        case PlatformInputKind::MouseDown:
            DispatchMouseDown(win, input->mouseDown);
            break;
        case PlatformInputKind::MouseUp:
            DispatchMouseUp(win, input->mouseUp);
            break;
        case PlatformInputKind::MouseMove:
            DispatchMouseMove(win, input->mouseMove);
            break;
        case PlatformInputKind::MouseExited:
            DispatchMouseExited(win, input->mouseExited);
            break;
        case PlatformInputKind::ScrollWheel:
            DispatchScrollWheel(win, input->scrollWheel);
            break;
    }
}

PlatformInput InputMouseDown(MouseButton button, float x, float y,
                             Modifiers modifiers, int clickCount,
                             bool firstMouse) {
    PlatformInput in = {};
    in.kind = PlatformInputKind::MouseDown;
    in.mouseDown.button = button;
    in.mouseDown.x = x;
    in.mouseDown.y = y;
    in.mouseDown.modifiers = modifiers;
    in.mouseDown.clickCount = clickCount;
    in.mouseDown.firstMouse = firstMouse;
    return in;
}

PlatformInput InputMouseUp(MouseButton button, float x, float y,
                           Modifiers modifiers, int clickCount) {
    PlatformInput in = {};
    in.kind = PlatformInputKind::MouseUp;
    in.mouseUp.button = button;
    in.mouseUp.x = x;
    in.mouseUp.y = y;
    in.mouseUp.modifiers = modifiers;
    in.mouseUp.clickCount = clickCount;
    return in;
}

PlatformInput InputMouseMove(float x, float y, bool pressed,
                             MouseButton pressedButton, Modifiers modifiers) {
    PlatformInput in = {};
    in.kind = PlatformInputKind::MouseMove;
    in.mouseMove.x = x;
    in.mouseMove.y = y;
    in.mouseMove.pressed = pressed;
    in.mouseMove.pressedButton = pressedButton;
    in.mouseMove.modifiers = modifiers;
    return in;
}

PlatformInput InputMouseExited(float x, float y, bool pressed,
                               MouseButton pressedButton, Modifiers modifiers) {
    PlatformInput in = {};
    in.kind = PlatformInputKind::MouseExited;
    in.mouseExited.x = x;
    in.mouseExited.y = y;
    in.mouseExited.pressed = pressed;
    in.mouseExited.pressedButton = pressedButton;
    in.mouseExited.modifiers = modifiers;
    return in;
}

PlatformInput InputScrollWheel(float x, float y, float deltaX, float deltaY,
                               bool precise, Modifiers modifiers,
                               TouchPhase phase) {
    PlatformInput in = {};
    in.kind = PlatformInputKind::ScrollWheel;
    in.scrollWheel.x = x;
    in.scrollWheel.y = y;
    in.scrollWheel.deltaX = deltaX;
    in.scrollWheel.deltaY = deltaY;
    in.scrollWheel.precise = precise;
    in.scrollWheel.modifiers = modifiers;
    in.scrollWheel.phase = phase;
    return in;
}

// blink_cursor.rs: INTERVAL and PAUSE_DELAY.
static const int kBlinkIntervalMs = 500;
static const int kBlinkPauseMs = 300;

static BlinkCursor* BlinkGet(App* app, EntityId handle) {
    return (BlinkCursor*)EntityGet(app, handle);
}

// ─── tooltip overlay ──────────────────────────────────────────────────────
//
// crates/base/src/tooltip.rs TooltipOverlay. One per window, driven by the
// hover change above.

static const int kTooltipShowDelayMs = 500;   // Rust's SHOW_DELAY
static const int kTooltipGracePeriodMs = 300; // Rust's GRACE_PERIOD

TooltipOverlay::~TooltipOverlay() {
    StrFree(text);
}

static TooltipOverlay* TooltipGet(Window* win) {
    if (!win || !win->app) {
        return nullptr;
    }
    if (!win->tooltip.IsValid()) {
        win->tooltip = EntityNewRaw(win->app, new TooltipOverlay(), nullptr,
                                    &EntityDropT<TooltipOverlay>);
    }
    return (TooltipOverlay*)EntityGet(win->app, win->tooltip);
}

// The Listener a countdown calls back through, bound to the overlay's own
// entity, so a timer dies with the window that owns it.
static Listener TooltipListener(Window* win, void* fn) {
    Listener l;
    l.fn = fn;
    l.view = win->tooltip;
    return l;
}

// cancel_tasks. There is at most one countdown of each kind in flight, so
// dropping it is what Rust's epoch guard achieves.
static void TooltipCancel(Window* win, TooltipOverlay* t) {
    if (t->showTimer) {
        WindowCancelTimer(win, t->showTimer);
        t->showTimer = 0;
    }
    if (t->hideTimer) {
        WindowCancelTimer(win, t->hideTimer);
        t->hideTimer = 0;
    }
}

static void TooltipSetText(TooltipOverlay* t, Str text) {
    StrFree(t->text);
    t->text = StrDup(text);
}

void TooltipOverlay::OnShow(TooltipOverlay* self, Ctx* cx, const TickEvent*) {
    self->showTimer = 0;
    self->visible = true;
    Notify(cx);
}

void TooltipOverlay::OnHide(TooltipOverlay* self, Ctx* cx, const TickEvent*) {
    self->hideTimer = 0;
    self->visible = false;
    self->hadRecent = false;
    StrFree(self->text);
    self->text = {};
    Notify(cx);
}

void TooltipRequestShow(Window* win, Str text, Bounds triggerBounds) {
    TooltipOverlay* t = TooltipGet(win);
    if (!t) {
        return;
    }
    // Same trigger as the one already up: nothing to do, and re-arming would
    // make it flicker.
    if (t->visible && t->text.s && StrEqI(t->text, text)) {
        TooltipCancel(win, t);
        return;
    }
    bool wasVisible = t->visible;
    TooltipCancel(win, t);
    TooltipSetText(t, text);
    t->triggerBounds = triggerBounds;
    // Already reading one, or only just stopped: swap straight to the new one.
    // Making someone wait out the delay again for a tip they were mid-way
    // through is what had_recent_tooltip exists to prevent.
    if (wasVisible || t->hadRecent) {
        t->visible = true;
        return;
    }
    t->showTimer =
        WindowSetTimeout(win, kTooltipShowDelayMs,
                         TooltipListener(win, (void*)&TooltipOverlay::OnShow));
}

void TooltipRequestHide(Window* win) {
    TooltipOverlay* t = TooltipGet(win);
    if (!t) {
        return;
    }
    if (t->showTimer) {
        // Left before it ever appeared; there is nothing to grant a grace to.
        WindowCancelTimer(win, t->showTimer);
        t->showTimer = 0;
    }
    if (!t->visible || t->hideTimer) {
        return;
    }
    t->hadRecent = true;
    t->hideTimer =
        WindowSetTimeout(win, kTooltipGracePeriodMs,
                         TooltipListener(win, (void*)&TooltipOverlay::OnHide));
}

const TooltipOverlay* TooltipShowing(Window* win) {
    if (!win || !win->app || !win->tooltip.IsValid()) {
        return nullptr;
    }
    return (const TooltipOverlay*)EntityGet(win->app, win->tooltip);
}

void BlinkCursor::OnFlip(BlinkCursor* self, Ctx* cx, const TickEvent*) {
    if (self->paused) {
        return;
    }
    self->visible = !self->visible;
    Notify(cx);
}

void BlinkCursor::OnResume(BlinkCursor* self, Ctx* cx, const TickEvent*) {
    // The pause is over; pick blinking back up lit, as Rust does.
    self->paused = false;
    self->visible = true;
    Listener flip;
    flip.fn = (void*)&BlinkCursor::OnFlip;
    flip.view = cx->self;
    self->timer = WindowSetInterval(cx->win, kBlinkIntervalMs, flip);
    Notify(cx);
}

// The Listener a timer calls back through, bound to the cursor's own entity —
// which is what makes the timer die with it.
static Listener BlinkListener(EntityId handle, void* fn) {
    Listener l;
    l.fn = fn;
    l.view = handle;
    return l;
}

void BlinkStart(App* app, Window* win, EntityId* handle) {
    if (!app || !win || !handle) {
        return;
    }
    if (!handle->IsValid()) {
        // cx.new(|_| BlinkCursor::new())
        *handle = EntityNewRaw(app, new BlinkCursor(), nullptr,
                               &EntityDropT<BlinkCursor>);
    }
    BlinkCursor* b = BlinkGet(app, *handle);
    if (!b || b->timer) {
        return; // already blinking
    }
    b->paused = false;
    // Rust starts hidden and flips on the first tick; lit immediately is what
    // makes a click feel like it landed.
    b->visible = true;
    b->timer =
        WindowSetInterval(win, kBlinkIntervalMs,
                          BlinkListener(*handle, (void*)&BlinkCursor::OnFlip));
    AppInvalidate(win);
}

void BlinkStop(App* app, Window* win, EntityId* handle) {
    if (!app || !win || !handle || !handle->IsValid()) {
        return;
    }
    BlinkCursor* b = BlinkGet(app, *handle);
    if (!b) {
        return;
    }
    WindowCancelTimer(win, b->timer);
    b->timer = 0;
    b->paused = false;
    b->visible = false;
    AppInvalidate(win);
}

void BlinkPause(App* app, Window* win, EntityId* handle) {
    if (!app || !win || !handle || !handle->IsValid()) {
        return;
    }
    BlinkCursor* b = BlinkGet(app, *handle);
    if (!b || !b->timer) {
        return; // not blinking, nothing to keep solid
    }
    WindowCancelTimer(win, b->timer);
    b->paused = true;
    b->visible = true;
    b->timer =
        WindowSetTimeout(win, kBlinkPauseMs,
                         BlinkListener(*handle, (void*)&BlinkCursor::OnResume));
    AppInvalidate(win);
}

bool BlinkVisible(App* app, EntityId handle) {
    BlinkCursor* b = BlinkGet(app, handle);
    if (!b || !b->timer) {
        return false;
    }
    // Paused means solid, not hidden.
    return b->paused || b->visible;
}

void WindowTimerTick(Window* win) {
    if (!win) {
        return;
    }
    double now = TimeNow();
    bool repaint = false;

    // A snapshot of the count, so a timer armed by a handler runs next pass
    // rather than inside this one.
    int n = win->timers.len;
    for (int i = 0; i < n && i < win->timers.len; i++) {
        TimerSub& t = win->timers[i];
        if (t.dueAt > now) {
            continue;
        }
        Listener l = t.l;
        int ms = t.ms;
        if (t.repeat) {
            t.dueAt = now + (double)ms / 1000.0;
        } else {
            t.dueAt = 0; // swept below
        }
        TickEvent ev = {ms};
        ListenerCall(win->app, win, l, &ev);
        repaint = true;
    }

    // Drop the one-shots that fired, and any timer whose view is gone — the
    // lifetime Rust gets from Task being dropped with its entity.
    int keep = 0;
    for (int i = 0; i < win->timers.len; i++) {
        const TimerSub& t = win->timers[i];
        bool dead = t.dueAt <= 0 ||
                    (t.l.view.IsValid() && !EntityGet(win->app, t.l.view));
        if (dead) {
            continue;
        }
        win->timers[keep++] = win->timers[i];
    }
    win->timers.len = keep;

    if (win->anim || repaint) {
        AppInvalidate(win);
    }
    PlatSetTimer(win, WindowTimerMs(win));
}

int WindowChromeHit(Window* win, float x, float y) {
    if (!win) {
        return 0;
    }
    int id = HitTest(&win->paint, x, y);
    if (id == ClickWinMin || id == ClickWinMax || id == ClickWinClose ||
        id == ClickWinCaption) {
        return id;
    }
    return 0;
}

int WindowTimerMs(Window* win) {
    if (!win) {
        return 0;
    }
    // Milliseconds until the soonest thing that wants the window back, or 0
    // if nothing does.
    double now = TimeNow();
    double soonest = -1;
    if (win->anim || win->opts.anim) {
        soonest = now + 0.016;
    }
    for (int i = 0; i < win->timers.len; i++) {
        double due = win->timers[i].dueAt;
        if (due > 0 && (soonest < 0 || due < soonest)) {
            soonest = due;
        }
    }
    if (soonest < 0) {
        return 0;
    }
    int ms = (int)((soonest - now) * 1000.0 + 0.5);
    return ms > 0 ? ms : 1;
}

// ─── lifecycle ────────────────────────────────────────────────────────────

Window* WindowAlloc(App* app, WinOpts opts) {
    if (!app) {
        return nullptr;
    }
    Window* win = new Window();
    win->app = app;
    win->opts = opts;
    win->anim = opts.anim;
    // The factories and the font cache live on App; each window borrows them.
    win->paint.pa = app->paint;
    app->windows.Append(win);
    return win;
}

bool AppAnyWindowOpen(App* app) {
    if (!app) {
        return false;
    }
    for (int i = 0; i < app->windows.len; i++) {
        if (app->windows[i]->plat) {
            return true;
        }
    }
    return false;
}

void WindowClosed(Window* win) {
    if (!win) {
        return;
    }
    PaintTargetFree(&win->paint);
    win->plat = nullptr;
    win->running = false;
}

App* AppNew() {
    App* app = new App();
    app->paint = PaintAppNew();
    if (!app->paint) {
        delete app;
        return nullptr;
    }
    if (!PlatInit(app)) {
        PaintAppFree(app->paint);
        delete app;
        return nullptr;
    }
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
        PaintTargetFree(&w->paint);
        w->timers.Reset();
        WindowKeyedFree(w);
        delete w;
    }
    app->windows.Reset();
    PaintAppFree(app->paint);
    app->paint = nullptr;
    PlatShutdown(app);
    delete app;
    DestroyTempArena();
}

void AppRequestAnim(Window* win, bool on) {
    if (!win) {
        return;
    }
    win->anim = on;
    win->opts.anim = on;
    // WindowTimerMs answers 0 when nothing is left wanting the window back.
    PlatSetTimer(win, WindowTimerMs(win));
}

// ─── runtime command line ────────────────────────────────────────────────

static bool gGeomAsked = false;
static int gGeom[4] = {0, 0, 0, 0};

// "12,-3,960,921" -> four ints. Anything else leaves the request unset rather
// than opening a window somewhere surprising.
static bool ParseGeom(const char* s, int out[4]) {
    for (int i = 0; i < 4; i++) {
        if (i > 0) {
            if (*s != ',') {
                return false;
            }
            s++;
        }
        bool neg = false;
        if (*s == '-') {
            neg = true;
            s++;
        }
        int digits = 0;
        int v = 0;
        while (*s >= '0' && *s <= '9') {
            v = v * 10 + (*s - '0');
            s++;
            digits++;
            if (digits > 6) {
                return false;
            }
        }
        if (digits == 0) {
            return false;
        }
        out[i] = neg ? -v : v;
    }
    return *s == 0 && out[2] > 0 && out[3] > 0;
}

bool WindowGeomRequested(int* x, int* y, int* w, int* h) {
    if (!gGeomAsked) {
        return false;
    }
    *x = gGeom[0];
    *y = gGeom[1];
    *w = gGeom[2];
    *h = gGeom[3];
    return true;
}

int GpuiTakeRuntimeArgs(int argc, char** argv) {
    const char* kGeom = "-gpui-window=";
    int keep = 0;
    for (int i = 0; i < argc; i++) {
        const char* a = argv[i];
        size_t kGeomLen = strlen(kGeom);
        if (i > 0 && a && strncmp(a, kGeom, kGeomLen) == 0) {
            int g[4];
            if (ParseGeom(a + kGeomLen, g)) {
                gGeomAsked = true;
                for (int k = 0; k < 4; k++) {
                    gGeom[k] = g[k];
                }
            }
            continue;
        }
        argv[keep++] = argv[i];
    }
    for (int i = keep; i <= argc; i++) {
        argv[i] = nullptr;
    }
    return keep;
}

// crates/story/src/lib.rs create_new_window_with_size: a window never asks
// for more than 85% of the display, however big the caller's default is.
void WindowClampToDisplay(int* dipW, int* dipH, int screenW, int screenH) {
    if (screenW > 0 && *dipW > (int)(screenW * 0.85f)) {
        *dipW = (int)(screenW * 0.85f);
    }
    if (screenH > 0 && *dipH > (int)(screenH * 0.85f)) {
        *dipH = (int)(screenH * 0.85f);
    }
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

int AppRunView(Str title, int dipW, int dipH, EntityId root, App* app,
               WinOpts opts) {
    if (!WindowOpenView(app, title, dipW, dipH, root, opts)) {
        return 1;
    }
    int rc = AppRun(app);
    AppFree(app);
    return rc;
}

void AppClose(Window* win) {
    AppQuit(win);
}

bool AppIsMaximized(Window* win) {
    return win && win->maximized;
}

} // namespace gpui
