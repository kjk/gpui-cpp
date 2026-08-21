/* Everything a window does that is not the OS window: frame drawing, input
   dispatch, the app lifecycle. Window_win.cpp and Window_linux.cpp call in
   here; nothing here calls back out except through Platform.h. */

#include "gpui/platform.h"
#include "gpui/keymap.h"
#include "gpui/image.h"
#include "gpui/paint.h"
#include "base/focus_trap.h"
#include "base/text_selection.h"

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
    // One instant for the whole frame, which is what every transition in it
    // measures against, and the frame that has just been asked for: whatever
    // still has somewhere to go asks again below.
    win->frameNow = drawStart;
    win->animFrame = false;
    if (!PaintTargetBegin(&win->paint, native, pxW, pxH)) {
        return;
    }

    if (win->frameArena) {
        win->frameArena->Reset();
    } else {
        win->frameArena = ArenaNew();
    }
    ResetTempArena();
    // element_opacity starts at 1 each frame, the way GPUI's window does.
    win->paint.opacity = 1.f;
    win->paint.hits.Clear();
    win->paint.scrolls.Clear();
    win->paint.texts.Clear();
    win->paint.inputs.Clear();
    win->paint.textDocLen = 0;
    win->paint.selA = -1;
    win->paint.selB = -1;
    win->paint.hoverId = win->hoverId;
    win->paint.focusId = win->focusId;
    win->paint.mouseX = win->mouseX;
    win->paint.mouseY = win->mouseY;
    win->paint.picking = win->inspector.picking;
    win->paint.wantsAnimFrame = false;
    win->paint.pickHit = false;
    win->paint.paintDepth = 0;
    win->paint.hitParent = -1;
    win->paint.pickTier = 0;
    win->paint.pick = {};
    if (win->inspector.pending) {
        // The press is what this frame picks against, not the pointer.
        win->paint.mouseX = win->inspector.pendingX;
        win->paint.mouseY = win->inspector.pendingY;
    }
    win->paint.viewW = dipW;
    win->paint.viewH = dipH;
    TextMeasBeginFrame(&win->paint);
    // Whatever a trap asked for last frame has been settled; this frame's
    // containers ask again as they build.
    win->pendingTrap = 0;

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

    // AutoScroll's tick. Rust spawns a 16 ms background task per state; the
    // frame is that clock here, so one tick is one frame and the request for
    // the next keeps it running while the pointer stays out at the edge. The
    // selection is re-run at the pointer's last place, since the content has
    // moved under it.
    if (win->input && win->input->autoScroll.IsActive() && win->mouseDown) {
        InputState* s = win->input;
        float was = s->scrollY;
        s->scrollY += s->autoScroll.delta;
        if (s->scrollY < 0) {
            s->scrollY = 0;
        }
        float most = s->contentH - s->viewH;
        if (most < 0) {
            most = 0;
        }
        if (s->scrollY > most) {
            s->scrollY = most;
        }
        if (s->scrollY != was && s->autoScroll.hasLastDrag) {
            InputSelectTo(
                s, win->app, win,
                InputIndexForPosition(s, &win->paint, s->autoScroll.lastDrag.x,
                                      s->autoScroll.lastDrag.y));
        }
        WindowRequestAnimationFrame(win);
    }

    // The window's own selection, before the view builds: an application
    // only says Selectable() on its text, the way Rust has the window drive
    // every registered run.
    WindowSelectionApply(win);

    El* root = EntityRender(win->app, win, win->frameArena, win->root);

    const Theme& th = ThemeNow();
    CanvasClear(&win->paint, th.background);
    if (root) {
        LayoutEl(&win->paint, root, 0, 0, dipW, dipH, ThemeFontSize(),
                 th.foreground);
        FocusCollect(win, root);
        // A dialog that has just opened takes focus into itself, which is what
        // Rust gets from tracking focus on the trap container.
        FocusTrapApplyPending(win);
        PaintEl(&win->paint, root);
    }
    // A Scrolling scrollbar part-way through its fade wants the next frame.
    // One ask for the whole tree, after it has painted, the way Rust's
    // scrollbar schedules its own idle timer.
    if (win->paint.wantsAnimFrame) {
        WindowRequestAnimationFrame(win);
    }
    // Rust renders TooltipOverlay deferred with priority 2, so the tip is over
    // everything the frame drew. It is the overlay's, not the trigger's: by
    // the time the show countdown lands, the frame that asked for it is gone.
    TooltipPaint(&win->paint, TooltipShowing(win));

    // The element the pointer is over while picking, and the one already
    // picked: GPUI paints the same two highlights over everything.
    if (win->inspector.on) {
        const Theme& ith = ThemeNow();
        if ((win->inspector.picking || win->inspector.pending) &&
            win->paint.pickHit) {
            Bounds b = win->paint.pick.bounds;
            FillRound(&win->paint, b.x, b.y, b.w, b.h, 0,
                      RgbaOpacity(ith.blue, 0.2f));
            DrawRoundStroke(&win->paint, b.x, b.y, b.w, b.h, 0, 1, ith.blue);
        } else if (win->inspector.hasPick) {
            Bounds b = win->inspector.pick.bounds;
            DrawRoundStroke(&win->paint, b.x, b.y, b.w, b.h, 0, 1, ith.blue);
        }
    }

    PaintTargetEnd(&win->paint);
    TextMeasEndFrame(&win->paint);

    // The pick a press asked for is settled against the frame it aimed at.
    if (win->inspector.pending) {
        if (win->paint.pickHit) {
            win->inspector.pick = win->paint.pick;
            win->inspector.hasPick = true;
        }
        win->inspector.pending = false;
        win->inspector.picking = false;
        AppInvalidate(win);
    }

    // The transitions of anything the frame did not build are dropped, which
    // is GPUI's element state going with the element. Something that comes
    // back on screen starts its entrance again rather than resuming one.
    WindowMotionSweep(win);

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

// The press is this window's for as long as the button is down: GPUI grabs
// the pointer so a drag can leave the window and still be heard, and the
// release that ends it is the one that must never go missing.
static void SetMouseDown(Window* win, bool down) {
    if (win->mouseDown == down) {
        return;
    }
    win->mouseDown = down;
    PlatSetMouseCapture(win, down);
}

static bool SliderKeyStep(Window* win, int key, bool ctrl, bool alt);

void WindowKeyDown(Window* win, int key, bool shift, bool ctrl, bool alt) {
    if (!win) {
        return;
    }
    // The focused field gets the chord first, as GPUI dispatches an action to
    // whatever has focus before anything else sees the key. The view's own
    // subscription still hears it — that is Rust's cx.propagate(), which every
    // action the input does not consume ends with — but a key the field ate is
    // not also an Enter on the focused element.
    //
    // Unless a sequence is half-finished: the rest of a binding written as
    // "ctrl-k ctrl-o" belongs to the keymap and to nothing else, which is
    // what GPUI's matcher running ahead of the text input buys. Both the
    // field and the page's own Copy stand aside for it.
    bool held = KeymapPending();
    win->eatChar = false;
    bool eaten = false;
    if (!held && win->input && win->input->focused) {
        InputAction action =
            InputActionForKey(win->input, key, shift, ctrl, alt);
        eaten = InputPerform(win->input, win->app, win, action, shift);
    }
    // Copy, once the focused field has had its go: a field with a selection
    // of its own copied that, and this is the page's selection — Rust's
    // TextSelection::selected_text, on the same chord. Nothing selected
    // leaves the key to whatever else wants it.
    if (!held && !eaten && key == KeyC && ctrl && !shift && !alt) {
        eaten = WindowSelectionCopy(win);
    }
    // The focused slider's arrows, before anything else looks at them: an
    // element bound to a SliderState is a slider whatever else it is.
    if (!held && !eaten && SliderKeyStep(win, key, ctrl, alt)) {
        win->eatChar = true;
        return;
    }
    // div().on_key_down: the focused element's own listener, and then the
    // ones above it, before the keymap resolves the chord. A field that is
    // not a text editor reads keys here — the keymap has no action to give it
    // and `win->input` takes an InputState, which an OTP field is not.
    if (!held && !eaten) {
        KeyEvent kd = {};
        kd.vk = key;
        kd.down = true;
        kd.shift = shift;
        kd.ctrl = ctrl;
        kd.alt = alt;
        if (WindowDispatchKeyEvent(win, &kd)) {
            // The character it also arrives as belongs to the handler that
            // took the key, not to whatever is under it.
            win->eatChar = true;
            AppInvalidate(win);
            return;
        }
    }
    // The keymap, once the focused field has had its go: a field's own
    // editing is Rust's innermost key context, so a binding further out
    // cannot take a keystroke away from it. An action that is handled ends
    // the keystroke here.
    if (!eaten && WindowDispatchKeyAction(win, key, shift, ctrl, alt)) {
        // The character the keystroke also arrives as is the keymap's now:
        // the second chord of a sequence is an ordinary letter, and typing it
        // into the field underneath is what the binding was there to stop.
        win->eatChar = true;
        win->eatReturn = false;
        AppInvalidate(win);
        return;
    }
    // The focus ring, last of the three: `tab` is bound on the window in
    // GPUI, the outermost key context there is, so a field indenting with it
    // and a binding over it both come first. Only a tab nobody wanted walks
    // the focus.
    if (!eaten && key == KeyTab) {
        FocusTrapTab(win, shift);
        AppInvalidate(win);
        return;
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
    // Enter and Space both activate the focused element, and the press only
    // arms that: the click is made from the release, the same as the mouse's.
    // GPUI keeps the focus generation the keystroke went down at as
    // `pending_keyboard_down`, and its key-down listener clears it for every
    // other key — a chord that ran an action is not half of an activation.
    // A focused field takes the space as text instead, so it never arms.
    bool activates = (key == KeyReturn && !win->eatReturn) ||
                     (key == KeySpace && !(win->input && win->input->focused));
    bool modified = shift || ctrl || alt;
    win->keyPressPending = activates && !modified && !eaten && win->focusId;
    win->keyPressGen = win->focusGen;
    win->eatReturn = false;
    AppInvalidate(win);
}

void WindowKeyUp(Window* win, int key, bool shift, bool ctrl, bool alt) {
    if (!win) {
        return;
    }
    // The release consumes the pending press whatever it is: a clean
    // activation makes the click, and anything else — another key coming up
    // mid-press, a modifier that has since gone down — cancels it.
    bool pending = win->keyPressPending;
    int gen = win->keyPressGen;
    win->keyPressPending = false;
    if (!ClickFromKeyRelease(pending, gen, win->focusGen, key,
                             shift || ctrl || alt)) {
        return;
    }
    // GPUI registers the keyboard activation on the painted element, so a
    // focus with nothing on screen behind it activates nothing.
    const HitRect* focused = HitRectById(win, win->focusId);
    if (!focused) {
        return;
    }
    // ClickEvent::Keyboard: no pointer was involved, so the position is the
    // element's own box, and there is no count or modifier to carry.
    ClickEvent ev = {0, 0, MouseButton::Left, win->focusId};
    ev.keyboard = true;
    ev.keyboardKey = key;
    ev.x = focused->bounds.CenterX();
    ev.y = focused->bounds.CenterY();
    ev.el = focused->bounds;
    // Both halves of what a click on it would have run, and only those: a
    // keyboard click reaches the element's own listeners, never the window's
    // unhandled-click path — nothing was clicked outside anything.
    if (focused->listener.IsValid()) {
        ListenerCall(win->app, win, focused->listener, &ev);
    }
    if (focused->onClick.IsValid()) {
        focused->onClick.Call();
    }
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
    bool ate = win->eatChar;
    win->eatChar = false;
    if (!ate && win->input && win->input->focused && ch >= 32 && ch != 127 &&
        !ctrl && !alt) {
        InputTypeChar(win->input, win->app, win, ch);
        ate = true;
    }
    // The focused element's own key listener hears the character half too:
    // a digit typed into an OTP field arrives as a WM_CHAR and never as a
    // chord the keymap could resolve.
    if (!ate && ch >= 32 && ch != 127 && !ctrl && !alt) {
        KeyEvent kd = {};
        kd.ch = ch;
        kd.down = true;
        if (WindowDispatchKeyEvent(win, &kd)) {
            AppInvalidate(win);
            return;
        }
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

// Whether a box scrolls at all along one axis, which is what decides if it
// has a bar to aim at.
static bool ScrollsY(const ScrollRect& s) {
    return s.contentH > s.bounds.h + 1.f;
}
static bool ScrollsX(const ScrollRect& s) {
    return s.contentW > s.bounds.w + 1.f;
}

// An offset that stays inside the content, which is Rust's clamp on the
// scroll handle rather than anything the wheel does.
static float ClampScroll(float off, float content, float viewport) {
    float most = content - viewport;
    if (most < 0) {
        most = 0;
    }
    if (off > most) {
        off = most;
    }
    return off < 0 ? 0 : off;
}

// The scrolled box whose scrollbar band the pointer is in, or null, and which
// of its two bars. Innermost first, the way the hit test reads its rects.
static const ScrollRect* ScrollbarAt(PaintCtx* ctx, float x, float y,
                                     bool* horizontal) {
    for (int i = ctx->scrolls.len - 1; i >= 0; i--) {
        const ScrollRect& s = ctx->scrolls[i];
        if (!s.onScroll.IsValid()) {
            continue;
        }
        if (ScrollsY(s) && y >= s.bounds.y && y <= s.bounds.Bottom() &&
            x >= s.bounds.Right() - kScrollbarBandW && x <= s.bounds.Right()) {
            *horizontal = false;
            return &ctx->scrolls[i];
        }
        if (ScrollsX(s) && x >= s.bounds.x && x <= s.bounds.Right() &&
            y >= s.bounds.Bottom() - kScrollbarBandW &&
            y <= s.bounds.Bottom()) {
            *horizontal = true;
            return &ctx->scrolls[i];
        }
    }
    return nullptr;
}

static void ScrollbarEmit(Window* win, const ScrollRect* s, float offsetX,
                          float offsetY) {
    ScrollEvent ev = {s->id, offsetY, offsetX};
    ListenerCall(win->app, win, s->onScroll, &ev);
    AppInvalidate(win);
}

// The press. Inside the thumb it opens a drag and keeps where it landed;
// anywhere else on the track the thumb jumps its centre to the press, which
// is Rust's two branches on `thumb_bounds.contains`. Both bars go through
// this once, along whichever axis they are.
static void ScrollbarPress(Window* win, const ScrollRect* s, float x, float y,
                           bool horizontal) {
    float track = horizontal ? s->bounds.w : s->bounds.h;
    float content = horizontal ? s->contentW : s->contentH;
    float origin = horizontal ? s->bounds.x : s->bounds.y;
    float at = horizontal ? x : y;
    float thumb = ScrollbarThumbSize(track, track, content);
    float thumbStart =
        origin + ScrollbarThumbPos(track, thumb,
                                   horizontal ? s->scrollX : s->scrollY, track,
                                   content);
    win->scrollDragId = s->id;
    win->scrollDragHorizontal = horizontal;
    if (at >= thumbStart && at <= thumbStart + thumb) {
        win->scrollDragGrab = at - thumbStart;
        return;
    }
    // A track press grabs the thumb by its middle, so the drag that may
    // follow carries on from where it just landed.
    win->scrollDragGrab = thumb * 0.5f;
    float off =
        ScrollbarOffsetForTrackPress(at, origin, track, thumb, track, content);
    ScrollbarEmit(win, s, horizontal ? off : s->scrollX,
                  horizontal ? s->scrollY : off);
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

static void ScrollbarDrag(Window* win, float x, float y) {
    const ScrollRect* s = ScrollRectById(win, win->scrollDragId);
    if (!s || !s->onScroll.IsValid()) {
        return;
    }
    bool horizontal = win->scrollDragHorizontal;
    float track = horizontal ? s->bounds.w : s->bounds.h;
    float content = horizontal ? s->contentW : s->contentH;
    float origin = horizontal ? s->bounds.x : s->bounds.y;
    float at = horizontal ? x : y;
    float thumb = ScrollbarThumbSize(track, track, content);
    float off = ScrollbarOffsetForDrag(at, win->scrollDragGrab, origin, track,
                                       thumb, track, content);
    ScrollbarEmit(win, s, horizontal ? off : s->scrollX,
                  horizontal ? s->scrollY : off);
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

// slider.rs's `on_a11y_action(Increment | Decrement)`, on the keyboard. There
// is no accessibility layer in this tree for the role and the aria values to
// live in, and the half that is reachable is the half a keyboard user needs:
// the arrows over the focused track, stepping by the slider's own step. Which
// end of a range moves is the one the last press took, which is what a reader
// who just dragged one of them expects to keep moving.
static bool SliderKeyStep(Window* win, int key, bool ctrl, bool alt) {
    if (ctrl || alt || !win->focusId) {
        return false;
    }
    int dir = 0;
    if (key == KeyRight || key == KeyUp) {
        dir = 1;
    } else if (key == KeyLeft || key == KeyDown) {
        dir = -1;
    }
    if (dir == 0) {
        return false;
    }
    for (int i = 0; i < win->paint.hits.len; i++) {
        const HitRect& hr = win->paint.hits[i];
        if (hr.id != win->focusId || !hr.slider) {
            continue;
        }
        if (SliderStepBy(hr.slider, dir,
                         hr.slider->value.range && hr.slider->dragStart)) {
            SliderEmit(win, hr.slider, SliderEventKind::Change);
        }
        // The keystroke was the slider's whether or not it could move: an
        // arrow on a slider at its limit is not also a walk of the focus.
        AppInvalidate(win);
        return true;
    }
    return false;
}

// How far the pointer travels before a press counts as a drag rather than a
// click. GPUI starts the drag from the first move that leaves the press
// behind; a few DIPs of slack is what a mouse gives a firm click.
static const float kDragThreshold = 4.f;

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
    // The drag half of the window's selection, before the view's own handler
    // so a page that watches moves sees the selection already extended.
    WindowSelectionDrag(win, x, y);
    if (win->onMouseMove.IsValid()) {
        ListenerCall(win->app, win, win->onMouseMove, &in);
    }
    // Whether this press has become a drag: GPUI starts one from the move
    // that leaves the press behind, not from the press itself.
    if (win->mouseDown && !win->pressedMoved) {
        float dx = x - win->pressedX;
        float dy = y - win->pressedY;
        if (dx * dx + dy * dy > kDragThreshold * kDragThreshold) {
            win->pressedMoved = true;
        }
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
        ScrollbarDrag(win, x, y);
    }
    // InputState::on_drag_move: the field that took the press keeps every move
    // until the release, wherever the pointer has got to. The button being
    // held is `win->mouseDown` rather than the move event's own flag, the same
    // signal the slider drag and on_drag_move above go by.
    if (win->input && win->input->selecting && win->mouseDown) {
        InputState* s = win->input;
        s->autoScroll.lastDrag = Point{x, y};
        s->autoScroll.hasLastDrag = true;
        InputSelectTo(s, win->app, win,
                      InputIndexForPosition(s, &win->paint, x, y));
        // A drag that has reached the edge of a field with somewhere to go
        // keeps scrolling it until the pointer comes back in. A single-line
        // field has nowhere to go, which is why Rust asks the same question.
        float delta = 0;
        if (!InputIsSingleLine(s) &&
            AutoScrollComputeDelta(y, s->inputBounds, &delta)) {
            s->autoScroll.Set(delta);
            AppInvalidate(win);
        } else {
            s->autoScroll.SetNone();
        }
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

// A press that something else took: whatever was pending stops being so, and
// the release that follows makes no click.
static void ClearPendingClick(Window* win) {
    win->pressPending = false;
    win->pressedId = 0;
    win->pressedMoved = false;
}

// ─── the dispatch chain ──────────────────────────────────────────────────

// cx.stop_propagation().
void WindowStopPropagation(Ctx* cx) {
    if (cx && cx->win) {
        cx->win->stopPropagation = true;
    }
}

// The chain of hit rects the pointer is inside, leaf first. Not every box
// that contains the point: two absolutely placed siblings can overlap without
// either being inside the other, so the chain is the one the paint recorded.
static void HitChain(Window* win, float x, float y, Vec<int>* out) {
    out->Clear();
    int leaf = -1;
    for (int i = win->paint.hits.len - 1; i >= 0; i--) {
        if (win->paint.hits[i].bounds.Contains({x, y})) {
            leaf = i;
            break;
        }
    }
    for (int i = leaf; i >= 0; i = win->paint.hits[i].parent) {
        out->Append(i);
    }
}

// Window::dispatch_event: the chain outside-in for the Capture phase, then
// inside-out for the Bubble phase, stopping wherever a handler said to.
// `pick` answers the handler an element registered, or an invalid Listener.
template <typename Ev, typename Pick>
static void DispatchChain(Window* win, const Vec<int>& chain, Ev* ev,
                          Pick pick) {
    win->stopPropagation = false;
    for (int k = chain.len - 1; k >= 0 && !win->stopPropagation; k--) {
        const HitRect& hr = win->paint.hits[chain[k]];
        Listener l = pick(hr, DispatchPhase::Capture);
        if (l.IsValid()) {
            ev->phase = DispatchPhase::Capture;
            ev->el = hr.bounds;
            ListenerCall(win->app, win, l, ev);
        }
    }
    for (int k = 0; k < chain.len && !win->stopPropagation; k++) {
        const HitRect& hr = win->paint.hits[chain[k]];
        Listener l = pick(hr, DispatchPhase::Bubble);
        if (l.IsValid()) {
            ev->phase = DispatchPhase::Bubble;
            ev->el = hr.bounds;
            ListenerCall(win->app, win, l, ev);
        }
    }
    win->stopPropagation = false;
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
        ClearPendingClick(win);
        AppInvalidate(win);
        return;
    }
    // The scrollbar sits over whatever it scrolls, so it is asked first: a
    // press on the bar is the bar's, not the row underneath it. Rust says the
    // same with cx.stop_propagation().
    // Inspector::is_picking: the press picks the element under the pointer
    // and goes no further, so the page it is over is not clicked.
    if (win->inspector.picking) {
        win->inspector.pending = true;
        win->inspector.pendingX = x;
        win->inspector.pendingY = y;
        ClearPendingClick(win);
        AppInvalidate(win);
        return;
    }
    bool barHorizontal = false;
    const ScrollRect* bar = ScrollbarAt(&win->paint, x, y, &barHorizontal);
    if (bar) {
        SetMouseDown(win, true);
        ScrollbarPress(win, bar, x, y, barHorizontal);
        // The bar took the press, so nothing is waiting to become a click.
        ClearPendingClick(win);
        AppInvalidate(win);
        return;
    }
    // The window's own selection hears the press first — Rust registers its
    // handler on the window, above every participant — and only acts where
    // there is selectable text. A press anywhere else drops what was
    // selected, which is the outside click that clears it.
    WindowSelectionPress(win, x, y, in.clickCount, in.modifiers.shift);

    const HitRect* hit = HitTestRect(&win->paint, x, y);
    int id = hit ? hit->id : 0;
    SetMouseDown(win, true);
    win->pressedId = id;
    // window.active_drag: a press on an element with a payload starts the
    // drag, and it lasts until the button comes back up.
    win->activeDrag = hit ? hit->drag : DragPayload{};
    win->dragOverId = 0;
    // cursor_offset, which GPUI takes as the drag starts: how far into the
    // element the press was.
    win->dragOffX = hit ? x - hit->bounds.x : 0;
    win->dragOffY = hit ? y - hit->bounds.y : 0;
    // A press takes focus only where there is a focus handle to take. Rust
    // gives a disabled widget its element id all the same — `div().id(id)` is
    // what makes it hit-testable and hoverable — and hangs `track_focus` off
    // `when(!disabled)`, so pressing one leaves focus where it was.
    if (id && FocusIdIsFocusable(win, id)) {
        WindowSetFocusId(win, id);
    }
    // on_mouse_down, ahead of the click: an element that wants the press
    // itself — a slider jumping to it — gets the whole event, not the
    // ClickEvent the click path builds.
    {
        // on_mouse_down over the whole chain, not just the element the press
        // landed on: a tile's frame hears the press its drag bar took, which
        // is what brings it to the front.
        Vec<int> chain;
        HitChain(win, x, y, &chain);
        MouseDownEvent ev = in;
        DispatchChain(
            win, chain, &ev, [](const HitRect& hr, DispatchPhase phase) {
                return hr.mouseDownPhase == phase ? hr.onMouseDown : Listener{};
            });
        chain.Reset();
    }
    if (hit && hit->slider) {
        SliderPress(win, hit, {x, y});
    }
    InputPress(win, in);
    // The click itself is not here: GPUI holds the press and fires on_click
    // from the release, on the element that took both. DispatchMouseUp does
    // that; what the press leaves behind is pressedId and the count.
    win->pressPending = true;
    win->pressedCount = in.clickCount;
    win->pressedX = x;
    win->pressedY = y;
    win->pressedMoved = false;
    win->pressedButton = in.button;
    win->pressedModifiers = in.modifiers;
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
    SetMouseDown(win, false);
    // with_unset_drag_pos: the release ends the scrollbar drag wherever it
    // landed.
    win->scrollDragId = 0;
    win->scrollDragGrab = 0;
    WindowSelectionRelease(win);
    if (win->onMouseUp.IsValid()) {
        ListenerCall(win->app, win, win->onMouseUp, &in);
    }
    // The element under the pointer hears the release, then the one that took
    // the press stops being held. A drag that ended somewhere else leaves the
    // first of those empty, which is what on_mouse_up_out is for.
    const HitRect* hit = HitTestRect(&win->paint, in.x, in.y);
    {
        Vec<int> chain;
        HitChain(win, in.x, in.y, &chain);
        MouseUpEvent ev = in;
        DispatchChain(
            win, chain, &ev, [](const HitRect& hr, DispatchPhase phase) {
                return hr.mouseUpPhase == phase ? hr.onMouseUp : Listener{};
            });
        chain.Reset();
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
    // A drag that actually happened takes the release: GPUI hands the up to
    // the drop and the click never runs. A press that picked a payload up and
    // never went anywhere is still a click.
    bool dragged = win->activeDrag.IsValid() && win->pressedMoved;
    if (dragged) {
        const HitRect* target =
            HitTestDrop(&win->paint, in.x, in.y, win->activeDrag.kind);
        if (target) {
            DropEvent ev = {win->activeDrag, in.x, in.y, target->bounds};
            ListenerCall(win->app, win, target->onDrop, &ev);
        }
    }
    // active_drag.take(): the drag is over whether or not it went anywhere.
    // A press that picked a payload up and let go without moving used to
    // leave it behind, and whatever draws from it — the dragged tab's
    // preview, a drop target's highlight — kept drawing.
    win->activeDrag = {};
    win->dragOverId = 0;
    SliderRelease(win);
    // InputState::on_mouse_up: the drag is over, and the word a double click
    // pinned stops holding the selection open.
    if (win->input && win->input->selecting) {
        win->input->selecting = false;
        win->input->hasSelectedWordRange = false;
        win->input->autoScroll.Stop();
    }
    // The click, last: GPUI's on_click fires from the release, and only when
    // the same button that went down comes up over the element that took it.
    // A press that slid off somewhere else is no click at all — which is what
    // lets a reader change their mind by moving off the button before
    // letting go.
    int upId = hit ? hit->id : 0;
    if (ClickFromRelease(win->pressPending, win->pressedId, win->pressedButton,
                         dragged, upId, in.button)) {
        ClickEvent ev = {in.x, in.y, in.button, win->pressedId};
        ev.clickCount = win->pressedCount;
        ev.modifiers = win->pressedModifiers;
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
    }
    ClearPendingClick(win);
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
    // A multi-line field takes the wheel before anything around it, the way
    // the editor's own scroll handle does in Rust.
    InputState* field = InputAtPosition(&win->paint, in.x, in.y);
    if (field && InputIsMultiLine(field) && field->contentH > field->viewH) {
        field->scrollY = ClampScroll(field->scrollY - in.deltaY,
                                     field->contentH, field->viewH);
        AppInvalidate(win);
        return;
    }
    // The scrolled box under the pointer takes the wheel, which is what a
    // `div().overflow_scroll()` does in GPUI — the offset is the view's here,
    // so the box reports where it should now be rather than moving itself.
    // Only a box that asked for the event gets it; anything else falls
    // through to the window subscription, as it did before there were any.
    for (int i = win->paint.scrolls.len - 1; i >= 0; i--) {
        const ScrollRect& s = win->paint.scrolls[i];
        if (!s.onScroll.IsValid() || !s.bounds.Contains({in.x, in.y})) {
            continue;
        }
        bool canY = ScrollsY(s);
        bool canX = ScrollsX(s);
        if (!canY && !canX) {
            continue;
        }
        // A wheel with no sideways delta over a box that only scrolls
        // sideways scrolls it anyway, which is what a mouse without a tilt
        // wheel needs.
        float dx = in.deltaX;
        float dy = in.deltaY;
        if (canX && !canY && dx == 0) {
            dx = dy;
            dy = 0;
        }
        float offY = canY ? ClampScroll(s.scrollY - dy, s.contentH, s.bounds.h)
                          : s.scrollY;
        float offX = canX ? ClampScroll(s.scrollX - dx, s.contentW, s.bounds.w)
                          : s.scrollX;
        if (offX == s.scrollX && offY == s.scrollY) {
            AppInvalidate(win);
            return;
        }
        ScrollEvent ev = {s.id, offY, offX};
        ListenerCall(win->app, win, s.onScroll, &ev);
        AppInvalidate(win);
        return;
    }
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

    if (win->anim || win->animFrame || repaint) {
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
    if (win->anim || win->opts.anim || win->animFrame) {
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

// Defined with AppOnShutdown below, and called from here.
static void AppRunShutdownFns();

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
        WindowSelectionFree(w);
        PaintTargetFree(&w->paint);
        w->timers.Reset();
        WindowKeyedFree(w);
        WindowMotionFree(w);
        delete w;
    }
    app->windows.Reset();
    // The decoded images outlive a window but not the backend that made
    // them, so they go before it does.
    ImageCacheClear();
    ScrollFadeClear();
    StyleOverrideClearAll();
    AppRunShutdownFns();
    PaintAppFree(app->paint);
    app->paint = nullptr;
    PlatShutdown(app);
    delete app;
    DestroyTempArena();
}

// window.request_animation_frame(). The flag is cleared as the next frame
// starts, so a caller that still has somewhere to go asks again while it
// renders, and one that has arrived stops.
void WindowRequestAnimationFrame(Window* win) {
    if (!win || win->animFrame) {
        return;
    }
    win->animFrame = true;
    // Nothing else may be keeping the window awake: arm the clock now, the
    // way AppRequestAnim does.
    PlatSetTimer(win, WindowTimerMs(win));
}

// The teardowns src/base and src/ui have registered, in the order they came.
static const int kMaxShutdownFns = 16;
static void (*gShutdownFns[kMaxShutdownFns])() = {};
static int gShutdownFnN = 0;

void AppOnShutdown(void (*fn)()) {
    if (!fn || gShutdownFnN >= kMaxShutdownFns) {
        return;
    }
    for (int i = 0; i < gShutdownFnN; i++) {
        if (gShutdownFns[i] == fn) {
            return;
        }
    }
    gShutdownFns[gShutdownFnN++] = fn;
}

static void AppRunShutdownFns() {
    for (int i = 0; i < gShutdownFnN; i++) {
        gShutdownFns[i]();
    }
    gShutdownFnN = 0;
}

void AppRefreshWindows(App* app) {
    if (!app) {
        return;
    }
    for (int i = 0; i < app->windows.len; i++) {
        AppInvalidate(app->windows[i]);
    }
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
