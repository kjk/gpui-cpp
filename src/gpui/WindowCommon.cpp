/* Everything a window does that is not the OS window: frame drawing, input
   dispatch, the app lifecycle. Window_win.cpp and Window_linux.cpp call in
   here; nothing here calls back out except through Platform.h. */

#include "gpui/Platform.h"
#include "gpui/Paint.h"

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
    win->paint.textDocLen = 0;
    win->paint.selA = -1;
    win->paint.selB = -1;
    win->paint.hoverId = win->hoverId;
    win->paint.focusId = win->focusId;
    win->paint.viewW = dipW;
    win->paint.viewH = dipH;
    TextMeasBeginFrame(&win->paint);

    El* root = EntityRender(win->app, win, win->frameArena, win->root);

    // Whatever the view pointed win->input at is the focused field. Start its
    // caret and stop the one that lost focus, so no app has to. Rust hangs
    // this off InputState::on_focus / on_blur.
    if (win->input != win->prevInput) {
        if (win->prevInput) {
            BlinkStop(win->app, win, &win->prevInput->blink);
        }
        if (win->input) {
            BlinkStart(win->app, win, &win->input->blink);
        }
        win->prevInput = win->input;
    }

    const Theme& th = ThemeNow();
    CanvasClear(&win->paint, th.background);
    if (root) {
        LayoutEl(&win->paint, root, 0, 0, dipW, dipH, 16.f, th.foreground);
        FocusCollect(win, root);
        PaintEl(&win->paint, root);
    }

    PaintTargetEnd(&win->paint);
    TextMeasEndFrame(&win->paint);

    // Record the frame for the trace. GPUI times Window::draw, which is this
    // whole function: build the element tree, lay it out, paint it.
    FrameTiming timing;
    timing.drawSecs = (float)(TimeNow() - drawStart);
    win->frameTrace[win->frameSeq % (uint64_t)kFrameTraceCap] = timing;
    win->frameSeq++;
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
    // Moving the caret is activity, so it stays solid then too.
    if (win->input && (key == KeyLeft || key == KeyRight || key == KeyUp ||
                       key == KeyDown || key == KeyHome || key == KeyEnd ||
                       key == KeyBack || key == KeyDelete)) {
        BlinkPause(win->app, win, &win->input->blink);
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
    // Enter activates the focused element: run that element's own listener,
    // the one a click on it would have run.
    if (key == KeyReturn && win->focusId && !win->eatReturn) {
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
            ev.elX = focused->x;
            ev.elY = focused->y;
            ev.elW = focused->w;
            ev.elH = focused->h;
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
    if (win->input && win->input->focused) {
        LineInput* in = win->input;
        bool changed = false;
        if (ch == 8) {
            if (in->len > 0) {
                in->len--;
                in->buf[in->len] = 0;
                in->cursor = in->len;
                changed = true;
            }
        } else if (ch >= 32 && ch < 127 && in->len < 511) {
            in->buf[in->len++] = (char)ch;
            in->buf[in->len] = 0;
            in->cursor = in->len;
            changed = true;
        }
        if (changed) {
            // Solid while the keys are coming, the way Rust's
            // pause_blink_cursor is called from every edit.
            BlinkPause(win->app, win, &in->blink);
        }
        // InputEvent::Change, for the view that subscribed to it.
        if (changed && in->onChange.IsValid()) {
            InputEvent ev = {InputEventKind::Change};
            ListenerCall(win->app, win, in->onChange, &ev);
        }
    }
    AppInvalidate(win);
}

void WindowMouseMove(Window* win, float x, float y) {
    if (!win) {
        return;
    }
    win->mouseX = x;
    win->mouseY = y;
    // An I-beam over anything selectable, the way every text view does it.
    // TextHitOffsetAt only answers for text that asked to be Selectable().
    CursorKind want = TextHitOffsetAt(&win->paint, x, y, false) >= 0
                          ? CursorKind::IBeam
                          : CursorKind::Arrow;
    if (want != win->cursor) {
        win->cursor = want;
        PlatSetCursor(win, want);
    }
    int id = HitTest(&win->paint, x, y);
    if (id != win->hoverId) {
        win->hoverId = id;
        AppInvalidate(win);
    }
    if (win->onMouse.IsValid()) {
        MouseEvent ev = {MouseKind::Move, x, y, 0, id};
        ListenerCall(win->app, win, win->onMouse, &ev);
    }
    if (win->mouseDown) {
        AppInvalidate(win);
    }
}

void WindowMouseDown(Window* win, float x, float y, int button) {
    if (!win) {
        return;
    }
    if (button == 2) {
        if (win->onMouse.IsValid()) {
            MouseEvent ev = {MouseKind::Down, x, y, 2, 0};
            ListenerCall(win->app, win, win->onMouse, &ev);
        }
        AppInvalidate(win);
        return;
    }
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
    ClickEvent ev = {x, y, 1, id};
    if (hit) {
        ev.elX = hit->x;
        ev.elY = hit->y;
        ev.elW = hit->w;
        ev.elH = hit->h;
    }
    if (hit && hit->listener.IsValid()) {
        ListenerCall(win->app, win, hit->listener, &ev);
    } else if (win->onClick.IsValid()) {
        ListenerCall(win->app, win, win->onClick, &ev);
    }
    if (hit && hit->onClick.IsValid()) {
        hit->onClick.Call();
    }
    AppInvalidate(win);
}

void WindowMouseUp(Window* win, float x, float y, int button) {
    if (!win) {
        return;
    }
    win->mouseDown = false;
    if (win->onMouse.IsValid()) {
        MouseEvent ev = {MouseKind::Up, x, y, button, 0};
        ListenerCall(win->app, win, win->onMouse, &ev);
    }
}

void WindowMouseLeave(Window* win) {
    if (!win) {
        return;
    }
    win->hoverId = 0;
    AppInvalidate(win);
}

void WindowWheel(Window* win, float x, float y, float delta) {
    if (!win) {
        return;
    }
    if (win->onWheel.IsValid()) {
        WheelEvent ev = {x, y, delta};
        ListenerCall(win->app, win, win->onWheel, &ev);
    }
    AppInvalidate(win);
}

void WindowDoubleClick(Window* win, float x, float y) {
    if (!win) {
        return;
    }
    int id = HitTest(&win->paint, x, y);
    // The title bar is 34 tall; a double click on empty chrome maximizes.
    if (id == ClickWinCaption || (id == 0 && y < 34)) {
        AppToggleMaximize(win);
    }
}

// blink_cursor.rs: INTERVAL and PAUSE_DELAY.
static const int kBlinkIntervalMs = 500;
static const int kBlinkPauseMs = 300;

static BlinkCursor* BlinkGet(App* app, EntityId handle) {
    return (BlinkCursor*)EntityGet(app, handle);
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
