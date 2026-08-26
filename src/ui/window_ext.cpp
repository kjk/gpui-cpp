#include "ui/window_ext.h"

namespace gpui {

WindowLayers* WindowLayersOf(Window* win) {
    if (!win) {
        return nullptr;
    }
    // window.use_keyed_state, which is the same lifetime Rust's Root has: the
    // window's, dropped with it.
    uint32_t key = (uint32_t)HashClickId(StrL("gpui-window-layers"));
    void* p = WindowKeyedState(win, key, new WindowLayers(),
                               &EntityDropT<WindowLayers>);
    return (WindowLayers*)p;
}

static WindowLayers* LayersOf(Ctx* cx) {
    return cx ? WindowLayersOf(cx->win) : nullptr;
}

// ─── dialogs ─────────────────────────────────────────────────────────────

void WindowOpenDialog(Ctx* cx, EntityId view, bool overlay) {
    WindowLayers* l = LayersOf(cx);
    if (!l || !EntityGet(cx->app, view)) {
        return;
    }
    // Opening the same dialog twice raises the one already up rather than
    // stacking a second copy of it.
    for (int i = 0; i < l->dialogs.len; i++) {
        if (l->dialogs[i].view == view) {
            return;
        }
    }
    WindowLayer layer;
    layer.view = view;
    layer.overlay = overlay;
    l->dialogs.Append(layer);
    Notify(cx);
}

bool WindowHasActiveDialog(Ctx* cx) {
    return WindowDialogCount(cx) > 0;
}

int WindowDialogCount(Ctx* cx) {
    WindowLayers* l = LayersOf(cx);
    return l ? l->dialogs.len : 0;
}

void WindowCloseDialog(Ctx* cx) {
    WindowLayers* l = LayersOf(cx);
    if (!l || l->dialogs.len == 0) {
        return;
    }
    l->dialogs.len--;
    EntityDrop(cx->app, l->dialogs[l->dialogs.len].view);
    Notify(cx);
}

void WindowCloseAllDialogs(Ctx* cx) {
    WindowLayers* l = LayersOf(cx);
    if (!l || l->dialogs.len == 0) {
        return;
    }
    for (int i = 0; i < l->dialogs.len; i++) {
        EntityDrop(cx->app, l->dialogs[i].view);
    }
    l->dialogs.len = 0;
    Notify(cx);
}

// ─── the sheet ───────────────────────────────────────────────────────────

void WindowOpenSheetAt(Ctx* cx, EntityId view,
                       component::SheetPlacement placement, float size) {
    WindowLayers* l = LayersOf(cx);
    if (!l || !EntityGet(cx->app, view)) {
        return;
    }
    if (l->hasSheet && l->sheet.view != view) {
        EntityDrop(cx->app, l->sheet.view);
    }
    l->sheet.view = view;
    l->sheet.placement = placement;
    l->sheet.size = size;
    l->hasSheet = true;
    Notify(cx);
}

bool WindowHasActiveSheet(Ctx* cx) {
    WindowLayers* l = LayersOf(cx);
    return l && l->hasSheet;
}

void WindowCloseSheet(Ctx* cx) {
    WindowLayers* l = LayersOf(cx);
    if (!l || !l->hasSheet) {
        return;
    }
    l->hasSheet = false;
    EntityDrop(cx->app, l->sheet.view);
    l->sheet = {};
    Notify(cx);
}

// ─── notifications ───────────────────────────────────────────────────────

Entity<component::NotificationListState> WindowNotifications(Ctx* cx) {
    WindowLayers* l = LayersOf(cx);
    if (!l) {
        return {};
    }
    if (!l->notifications.IsValid()) {
        l->notifications =
            EntityNewState<component::NotificationListState>(cx->app);
        if (component::NotificationListState* st = l->notifications.Get(cx)) {
            // What a system notification's response is dispatched back to,
            // stamped here as well as at render because a push can come
            // before the list has ever drawn.
            st->self = l->notifications.id;
        }
        // Rust spawns a task that advances the list every 50 ms; a window
        // timer is the same clock, and it is armed here rather than by an
        // application because the list is the window's.
        l->notifyTimer = WindowSetInterval(
            cx->win, component::kNotificationTickMs,
            ListenTo(l->notifications,
                     &component::NotificationListState::OnTick));
    }
    return l->notifications;
}

int WindowPushNotification(Ctx* cx, component::NotificationItem item,
                           int timeoutMs) {
    component::NotificationListState* st = WindowNotifications(cx).Get(cx);
    if (!st) {
        return 0;
    }
    int id = component::NotificationPush(st, cx, item, timeoutMs);
    Notify(cx);
    return id;
}

int WindowPushNotification(Ctx* cx, component::NotificationKind kind,
                           Str message) {
    component::NotificationItem item;
    item.kind = kind;
    item.message = message;
    // Notification::timeout, Duration::from_secs(5).
    return WindowPushNotification(cx, item, 5000);
}

void WindowClearNotifications(Ctx* cx) {
    WindowLayers* l = LayersOf(cx);
    if (!l || !l->notifications.IsValid()) {
        return;
    }
    if (component::NotificationListState* st = l->notifications.Get(cx)) {
        component::NotificationClear(st, cx);
        Notify(cx);
    }
}

int WindowNotificationCount(Ctx* cx) {
    WindowLayers* l = LayersOf(cx);
    if (!l || !l->notifications.IsValid()) {
        return 0;
    }
    component::NotificationListState* st = l->notifications.Get(cx);
    return st ? st->items.len : 0;
}

// ─── the focused input ───────────────────────────────────────────────────

InputState* WindowFocusedInput(Ctx* cx) {
    return cx && cx->win ? cx->win->input : nullptr;
}

bool WindowHasFocusedInput(Ctx* cx) {
    return WindowFocusedInput(cx) != nullptr;
}

} // namespace gpui
