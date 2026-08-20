/* Entity store, contexts and listeners — GPUI's App/Context in C++. */

#include "gpui/gpui.h"
#include "gpui/platform.h"

namespace gpui {

// Slot 0 is never handed out so a zeroed EntityId reads as null.
EntityId EntityNewRaw(App* app, void* ptr, RenderFn render, DropFn drop) {
    EntityId id;
    if (!app || !ptr) {
        return id;
    }
    int32_t ix;
    if (app->freeSlots.len > 0) {
        ix = app->freeSlots[app->freeSlots.len - 1];
        app->freeSlots.len--;
    } else {
        EntitySlot fresh = {};
        app->entities.Append(fresh);
        ix = (int32_t)(app->entities.len - 1);
    }
    EntitySlot& s = app->entities[ix];
    s.ptr = ptr;
    s.render = render;
    s.drop = drop;
    if (s.gen == 0) {
        s.gen = 1;
    }
    id.index = ix;
    id.gen = s.gen;
    return id;
}

void* EntityGet(App* app, EntityId id) {
    if (!app || !id.IsValid() || id.index >= app->entities.len) {
        return nullptr;
    }
    EntitySlot& s = app->entities[id.index];
    if (s.gen != id.gen || !s.ptr) {
        return nullptr;
    }
    return s.ptr;
}

void EntityDrop(App* app, EntityId id) {
    if (!app || !id.IsValid() || id.index >= app->entities.len) {
        return;
    }
    EntitySlot& s = app->entities[id.index];
    if (s.gen != id.gen || !s.ptr) {
        return;
    }
    if (s.drop) {
        s.drop(s.ptr);
    }
    s.ptr = nullptr;
    s.render = nullptr;
    s.drop = nullptr;
    // Bump the generation so every outstanding handle goes stale.
    s.gen++;
    if (s.gen == 0) {
        s.gen = 1;
    }
    app->freeSlots.Append(id.index);
}

void EntityDropAll(App* app) {
    if (!app) {
        return;
    }
    for (int i = 0; i < app->entities.len; i++) {
        EntitySlot& s = app->entities[i];
        if (s.ptr && s.drop) {
            s.drop(s.ptr);
        }
        s.ptr = nullptr;
        s.render = nullptr;
        s.drop = nullptr;
    }
    app->entities.Reset();
    app->freeSlots.Reset();
}

El* EntityRender(App* app, Window* win, Arena* a, EntityId id) {
    if (!app || !id.IsValid() || id.index >= app->entities.len) {
        return nullptr;
    }
    EntitySlot& s = app->entities[id.index];
    if (s.gen != id.gen || !s.ptr || !s.render) {
        return nullptr;
    }
    Ctx cx;
    cx.app = app;
    cx.win = win;
    cx.a = a;
    cx.self = id;
    return s.render(s.ptr, &cx);
}

const Theme& Ctx::theme() const {
    return themeMode() == ThemeMode::Dark ? ThemeDark() : ThemeLight();
}

ThemeMode Ctx::themeMode() const {
    return app ? app->themeMode : ThemeGet();
}

void NotifyApp(App* app) {
    if (!app) {
        return;
    }
    for (int i = 0; i < app->windows.len; i++) {
        AppInvalidate(app->windows[i]);
    }
}

void Notify(Ctx* cx) {
    if (!cx) {
        return;
    }
    if (cx->win) {
        AppInvalidate(cx->win);
        return;
    }
    NotifyApp(cx->app);
}

void ListenerCall(App* app, Window* win, const Listener& l, const void* ev) {
    if (!l.fn) {
        return;
    }
    void* self = EntityGet(app, l.view);
    if (!self) {
        // The view went away between paint and dispatch; GPUI drops it too.
        return;
    }
    Ctx cx;
    cx.app = app;
    cx.win = win;
    cx.a = win ? win->frameArena : nullptr;
    cx.self = l.view;
    if (l.hasArg) {
        ((ListenerArgFn)l.fn)(self, &cx, ev, l.arg);
    } else {
        ((ListenerFn)l.fn)(self, &cx, ev);
    }
}

void WindowOnKey(Window* win, Listener l) {
    if (win) {
        win->onKey = l;
    }
}

void WindowOnScrollWheel(Window* win, Listener l) {
    if (win) {
        win->onScrollWheel = l;
    }
}

WinSize WindowSize(Window* win) {
    WinSize ws = {};
    if (!win) {
        return ws;
    }
    ws.dipW = win->paint.viewW;
    ws.dipH = win->paint.viewH;
    ws.pxW = DipToPx(&win->paint, ws.dipW);
    ws.pxH = DipToPx(&win->paint, ws.dipH);
    return ws;
}

const DragPayload* WindowActiveDrag(Ctx* cx) {
    if (!cx || !cx->win || !cx->win->activeDrag.IsValid()) {
        return nullptr;
    }
    return &cx->win->activeDrag;
}

bool WindowIsActive(Ctx* cx) {
    return (cx && cx->win) ? cx->win->active : true;
}

void WindowSetActive(Window* win, bool active) {
    if (!win || win->active == active) {
        return;
    }
    win->active = active;
    AppInvalidate(win);
}

int WindowDragOverId(Ctx* cx) {
    return (cx && cx->win) ? cx->win->dragOverId : 0;
}

void WindowToggleInspector(Window* win) {
    if (!win) {
        return;
    }
    win->inspector.on = !win->inspector.on;
    // Toggling it on starts in picking mode, which is what the magnifier is
    // already pressed for in Rust.
    win->inspector.picking = win->inspector.on;
    if (!win->inspector.on) {
        win->inspector.hasPick = false;
    }
    AppInvalidate(win);
}

void WindowInspectorPick(Window* win, bool picking) {
    if (!win) {
        return;
    }
    win->inspector.picking = picking;
    AppInvalidate(win);
}

const InspectorState* WindowInspector(Ctx* cx) {
    return (cx && cx->win) ? &cx->win->inspector : nullptr;
}

void WindowOnUnhandledClick(Window* win, Listener l) {
    if (win) {
        win->onClick = l;
    }
}

void WindowOnMouseDown(Window* win, Listener l) {
    if (win) {
        win->onMouseDown = l;
    }
}

void WindowOnMouseUp(Window* win, Listener l) {
    if (win) {
        win->onMouseUp = l;
    }
}

void WindowOnMouseMove(Window* win, Listener l) {
    if (win) {
        win->onMouseMove = l;
    }
}

void WindowOnMouseExit(Window* win, Listener l) {
    if (win) {
        win->onMouseExit = l;
    }
}

static int WindowArmTimer(Window* win, int ms, Listener l, bool repeat) {
    if (!win || ms <= 0 || !l.IsValid()) {
        return 0;
    }
    TimerSub t;
    t.id = win->nextTimerId++;
    t.ms = ms;
    t.dueAt = TimeNow() + (double)ms / 1000.0;
    t.repeat = repeat;
    t.l = l;
    win->timers.Append(t);
    PlatSetTimer(win, WindowTimerMs(win));
    return t.id;
}

int WindowSetInterval(Window* win, int ms, Listener l) {
    return WindowArmTimer(win, ms, l, true);
}

int WindowSetTimeout(Window* win, int ms, Listener l) {
    return WindowArmTimer(win, ms, l, false);
}

void WindowCancelTimer(Window* win, int id) {
    if (!win || id <= 0) {
        return;
    }
    for (int i = 0; i < win->timers.len; i++) {
        if (win->timers[i].id != id) {
            continue;
        }
        for (int j = i + 1; j < win->timers.len; j++) {
            win->timers[j - 1] = win->timers[j];
        }
        win->timers.len--;
        break;
    }
    PlatSetTimer(win, WindowTimerMs(win));
}

void* WindowKeyedState(Window* win, uint32_t key, int size, DropFn drop) {
    if (!win || size <= 0) {
        return nullptr;
    }
    for (int i = 0; i < win->keyed.len; i++) {
        if (win->keyed[i].key == key) {
            return win->keyed[i].ptr;
        }
    }
    KeyedSlot s = {};
    s.key = key;
    s.ptr = AllocZero(1, size);
    s.drop = drop;
    win->keyed.Append(s);
    return s.ptr;
}

// window.use_keyed_state, when the state has to be an entity so timers and
// listeners can be bound to it. `fresh` is a new T the caller allocated; it is
// adopted on the first call for this key and deleted on every later one, which
// is how a C++ caller says Rust's `|_, cx| HoverCardState::new(..)` without a
// closure to defer it.
EntityId WindowKeyedEntity(Window* win, App* app, uint32_t key, void* fresh,
                           DropFn drop) {
    if (!win || !app) {
        if (fresh && drop) {
            drop(fresh);
        }
        return {};
    }
    for (int i = 0; i < win->keyed.len; i++) {
        if (win->keyed[i].key != key) {
            continue;
        }
        // Still ours only while the entity is alive; a recycled slot starts
        // over, the way a dropped keyed state does.
        if (EntityGet(app, win->keyed[i].entity)) {
            if (fresh && drop) {
                drop(fresh);
            }
            return win->keyed[i].entity;
        }
        win->keyed[i].entity = EntityNewRaw(app, fresh, nullptr, drop);
        return win->keyed[i].entity;
    }
    KeyedSlot s = {};
    s.key = key;
    s.entity = EntityNewRaw(app, fresh, nullptr, drop);
    win->keyed.Append(s);
    return s.entity;
}

void WindowKeyedFree(Window* win) {
    if (!win) {
        return;
    }
    for (int i = 0; i < win->keyed.len; i++) {
        // AllocZero'd, so free the memory without running a destructor. An
        // entity slot holds no memory of its own — the app owns that.
        if (win->keyed[i].ptr) {
            Free(nullptr, win->keyed[i].ptr);
        }
    }
    win->keyed.Reset();
}

} // namespace gpui
