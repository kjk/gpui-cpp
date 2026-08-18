/* Everything a window does that is not the OS window: frame drawing, input
   dispatch, the app lifecycle. Window_win.cpp and Window_linux.cpp call in
   here; nothing here calls back out except through Platform.h. */

#include "gpui/Platform.h"
#include "gpui/Paint.h"

namespace gpui {

static const int kTickMs = 500;

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

void WindowTimerTick(Window* win) {
    if (!win) {
        return;
    }
    if (win->onTick.IsValid()) {
        TickEvent ev = {win->tickMs};
        ListenerCall(win->app, win, win->onTick, &ev);
    }
    if (win->anim || win->onTick.IsValid()) {
        AppInvalidate(win);
    }
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
    if (win->anim) {
        return 16;
    }
    if (win->tickMs > 0) {
        return win->tickMs;
    }
    if (win->opts.anim) {
        return 16;
    }
    if (win->opts.timerMs > 0) {
        return win->opts.timerMs;
    }
    return kTickMs;
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
    if (!on && win->tickMs <= 0 && win->opts.timerMs <= 0) {
        PlatSetTimer(win, 0);
        return;
    }
    PlatSetTimer(win, WindowTimerMs(win));
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
