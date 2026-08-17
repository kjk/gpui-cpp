/* Entity store, contexts and listeners — GPUI's App/Context in C++. */

#include "gpui/Gpui.h"

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
    l.fn(self, &cx, ev);
}

void WindowOnKey(Window* win, Listener l) {
    if (win) {
        win->onKey = l;
    }
}

void WindowOnWheel(Window* win, Listener l) {
    if (win) {
        win->onWheel = l;
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

void WindowOnClick(Window* win, Listener l) {
    if (win) {
        win->onClick = l;
    }
}

void WindowOnMouse(Window* win, Listener l) {
    if (win) {
        win->onMouse = l;
    }
}

void WindowSetInterval(Window* win, int ms, Listener l) {
    if (!win) {
        return;
    }
    win->onTick = l;
    win->tickMs = ms;
    if (!win->hwnd) {
        return;
    }
    if (ms > 0) {
        SetTimer(win->hwnd, 1, (UINT)ms, nullptr);
    } else {
        KillTimer(win->hwnd, 1);
    }
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

void WindowKeyedFree(Window* win) {
    if (!win) {
        return;
    }
    for (int i = 0; i < win->keyed.len; i++) {
        // AllocZero'd, so free the memory without running a destructor.
        Free(nullptr, win->keyed[i].ptr);
    }
    win->keyed.Reset();
}

} // namespace gpui
