#include "shell/root.h"

#include "base/dialog.h"
#include "base/sheet.h"
#include "ui/root.h"
#include "ui/theme.h"
#include "ui/window_ext.h"

namespace gpui {

static uint32_t ShellRootWindowKey() {
    return (uint32_t)HashClickId(StrL("gpui-shell-root"));
}

struct ShellRootWindowState {
    EntityId root = {};
};

static ShellRootWindowState* RootWindowState(Window* window) {
    if (!window) return nullptr;
    return (ShellRootWindowState*)WindowKeyedState(
        window, ShellRootWindowKey(), new ShellRootWindowState(),
        &EntityDropT<ShellRootWindowState>);
}

const char* ToastLevelName(ToastLevel level) {
    switch (level) {
        case ToastLevel::Info: return "info";
        case ToastLevel::Success: return "success";
        case ToastLevel::Warning: return "warning";
        case ToastLevel::Error: return "error";
    }
    return "info";
}

bool ToastLevelFromName(Str name, ToastLevel* out) {
    if (StrEq(name, "info")) *out = ToastLevel::Info;
    else if (StrEq(name, "success")) *out = ToastLevel::Success;
    else if (StrEq(name, "warning")) *out = ToastLevel::Warning;
    else if (StrEq(name, "error")) *out = ToastLevel::Error;
    else return false;
    return true;
}

ShellRoot::~ShellRoot() {
    if (app && content.IsValid()) EntityDrop(app, content);
}

Entity<ShellRoot> ShellRoot::New(App* app, EntityId content) {
    Entity<ShellRoot> root = EntityNew<ShellRoot>(app);
    if (ShellRoot* state = root.Get(app)) {
        state->app = app;
        state->content = content;
    }
    return root;
}

El* ShellRoot::Render(ShellRoot* self, Ctx* cx) {
    if (!self) return Div(cx->a)->SizeFull();
    if (ShellRootWindowState* state = RootWindowState(cx->win))
        state->root = cx->self;
    El* content = self->content.IsValid()
                      ? EntityRender(cx->app, cx->win, cx->a, self->content)
                      : nullptr;
    return component::Root::New(cx)
        ->Bordered(false)
        ->Child(content ? content : Div(cx->a)->SizeFull())
        ->IntoEl();
}

ShellRoot* ShellRootOf(Window* window, App* app) {
    ShellRootWindowState* state = RootWindowState(window);
    if (!state || !state->root.IsValid() || state->root != window->root)
        return nullptr;
    return Entity<ShellRoot>{state->root}.Get(app);
}

static void RestoreOverlayFocus(Ctx* cx, FocusHandle restore) {
    if (!FocusHandleRestore(cx->win, restore)) WindowSetFocusId(cx->win, 0);
}

struct ShellDialogLayer {
    App* app = nullptr;
    Entity<ScriptView> content = {};
    DialogOptions options = {};
    FocusHandle focus = {};
    FocusHandle restore = {};

    ~ShellDialogLayer() {
        if (app && content.IsValid()) EntityDrop(app, content.id);
    }

    static void Close(ShellDialogLayer* self, Ctx* cx, const void*) {
        if (!self) return;
        FocusHandle restore = self->restore;
        WindowCloseDialog(cx);
        RestoreOverlayFocus(cx, restore);
    }

    static void OnBackdrop(ShellDialogLayer* self, Ctx* cx,
                           const MouseDownEvent* event) {
        if (!self || !event || event->button != MouseButton::Left ||
            !self->options.backdropDismissable)
            return;
        WindowLayers* layers = WindowLayersOf(cx->win);
        bool topmost = layers && layers->dialogs.len > 0 &&
                       layers->dialogs[layers->dialogs.len - 1].view ==
                           cx->self;
        if (!topmost) return;
        WindowStopPropagation(cx);
        Close(self, cx, event);
    }

    static El* Render(ShellDialogLayer* self, Ctx* cx) {
        const Theme& theme = ThemeNow(cx->app);
        WindowLayers* layers = WindowLayersOf(cx->win);
        bool topmost = layers && layers->dialogs.len > 0 &&
                       layers->dialogs[layers->dialogs.len - 1].view ==
                           cx->self;
        El* backdrop = nullptr;
        if (topmost) {
            backdrop = DialogBackdrop::New(cx)
                           ->Bg(Rgba8(0, 0, 0, 128))
                           ->OnMouseDown(Listen(cx, &ShellDialogLayer::OnBackdrop));
        }
        El* child = self->content.IsValid()
                        ? EntityRender(cx->app, cx->win, cx->a,
                                       self->content.id)
                        : nullptr;
        El* surface = Div(cx->a)
                          ->FlexCol()
                          ->Bg(theme.popover)
                          ->Fg(theme.popoverFg)
                          ->Border(1, theme.border)
                          ->Radius(theme.radiusLg)
                          ->Pad(16)
                          ->Child(child ? child : Div(cx->a));
        El* popup = DialogPopup::New(cx)
                        ->Absolute()
                        ->Top(0)
                        ->Left(0)
                        ->Right(0)
                        ->Bottom(0)
                        ->Flex()
                        ->ItemsCenter()
                        ->JustifyCenter()
                        ->Child(surface);
        Str trap = StrDup(cx->a, fmt("shell-dialog-%d", cx->self.index));
        if (topmost && self->options.escapeDismissable)
            DialogBindKeys(cx, popup, trap, {}, {},
                           Listen(cx, &ShellDialogLayer::Close));
        return Dialog::New(cx)
            ->Trap(trap)
            ->Backdrop(backdrop)
            ->Popup(popup)
            ->IntoEl()
            ->DeferredLayer((int)(10 + (layers ? layers->dialogs.len : 0)));
    }
};

int ShellRootOpenDialog(Ctx* cx, Entity<ScriptView> content,
                        DialogOptions options) {
    if (!cx || !content.IsValid() || !ShellRootOf(cx->win, cx->app)) return 0;
    Entity<ShellDialogLayer> layer = EntityNew<ShellDialogLayer>(cx->app);
    ShellDialogLayer* state = layer.Get(cx);
    if (!state) return 0;
    state->app = cx->app;
    state->content = content;
    state->options = options;
    state->focus = FocusHandleNew(cx);
    state->restore = WindowFocused(cx->win);
    WindowOpenDialog(cx, layer.id, true);
    FocusHandleFocus(cx->win, state->focus);
    return WindowDialogCount(cx);
}

bool ShellRootCloseDialog(Ctx* cx) {
    if (!cx || !ShellRootOf(cx->win, cx->app)) return false;
    WindowLayers* layers = WindowLayersOf(cx->win);
    if (!layers || layers->dialogs.len == 0) return false;
    Entity<ShellDialogLayer> layer{layers->dialogs[layers->dialogs.len - 1].view};
    ShellDialogLayer* state = layer.Get(cx);
    FocusHandle restore = state ? state->restore : FocusHandle{};
    WindowCloseDialog(cx);
    RestoreOverlayFocus(cx, restore);
    return true;
}

int ShellRootCloseAllDialogs(Ctx* cx) {
    if (!cx || !ShellRootOf(cx->win, cx->app)) return 0;
    WindowLayers* layers = WindowLayersOf(cx->win);
    int count = layers ? layers->dialogs.len : 0;
    if (count == 0) return 0;
    ShellDialogLayer* first =
        Entity<ShellDialogLayer>{layers->dialogs[0].view}.Get(cx);
    FocusHandle restore = first ? first->restore : FocusHandle{};
    WindowCloseAllDialogs(cx);
    RestoreOverlayFocus(cx, restore);
    return count;
}

bool ShellRootHasDialog(Ctx* cx) {
    return cx && ShellRootOf(cx->win, cx->app) && WindowHasActiveDialog(cx);
}

struct ShellSheetLayer {
    App* app = nullptr;
    Entity<ScriptView> content = {};
    component::SheetPlacement placement = component::SheetPlacement::Right;
    FocusHandle focus = {};
    FocusHandle restore = {};

    ~ShellSheetLayer() {
        if (app && content.IsValid()) EntityDrop(app, content.id);
    }

    static void Close(ShellSheetLayer* self, Ctx* cx, const void*) {
        if (!self) return;
        FocusHandle restore = self->restore;
        WindowCloseSheet(cx);
        RestoreOverlayFocus(cx, restore);
    }

    static El* Render(ShellSheetLayer* self, Ctx* cx) {
        const Theme& theme = ThemeNow(cx->app);
        El* child = self->content.IsValid()
                        ? EntityRender(cx->app, cx->win, cx->a,
                                       self->content.id)
                        : nullptr;
        WinSize size = WindowSize(cx->win);
        El* surface = Div(cx->a)
                          ->FlexCol()
                          ->Absolute()
                          ->Bg(theme.popover)
                          ->Fg(theme.popoverFg)
                          ->Border(1, theme.border)
                          ->Pad(16)
                          ->Child(child ? child : Div(cx->a));
        switch (self->placement) {
            case component::SheetPlacement::Left:
                surface->Top(0)->Bottom(0)->Left(0)->W(size.dipW / 3.f);
                break;
            case component::SheetPlacement::Right:
                surface->Top(0)->Bottom(0)->Right(0)->W(size.dipW / 3.f);
                break;
            case component::SheetPlacement::Top:
                surface->Top(0)->Left(0)->Right(0)->H(size.dipH / 3.f);
                break;
            case component::SheetPlacement::Bottom:
                surface->Bottom(0)->Left(0)->Right(0)->H(size.dipH / 3.f);
                break;
        }
        return Sheet::New(cx)
            ->Trap(StrL("shell-sheet"))
            ->Overlay(Div(cx->a)->Absolute()->Top(0)->Left(0)->Right(0)->Bottom(0)->Bg(
                Rgba8(0, 0, 0, 128)))
            ->Surface(surface)
            ->RequestClose(Listen(cx, &ShellSheetLayer::Close))
            ->IntoEl()
            ->DeferredLayer(5);
    }
};

bool ShellRootOpenSheet(Ctx* cx, Entity<ScriptView> content,
                        component::SheetPlacement placement) {
    if (!cx || !content.IsValid() || !ShellRootOf(cx->win, cx->app))
        return false;
    FocusHandle restore = WindowFocused(cx->win);
    WindowLayers* layers = WindowLayersOf(cx->win);
    if (layers && layers->hasSheet) {
        ShellSheetLayer* current =
            Entity<ShellSheetLayer>{layers->sheet.view}.Get(cx);
        if (current) restore = current->restore;
    }
    Entity<ShellSheetLayer> layer = EntityNew<ShellSheetLayer>(cx->app);
    ShellSheetLayer* state = layer.Get(cx);
    if (!state) return false;
    state->app = cx->app;
    state->content = content;
    state->placement = placement;
    state->focus = FocusHandleNew(cx);
    state->restore = restore;
    WinSize size = WindowSize(cx->win);
    float extent = (placement == component::SheetPlacement::Left ||
                    placement == component::SheetPlacement::Right)
                       ? size.dipW / 3.f
                       : size.dipH / 3.f;
    WindowOpenSheetAt(cx, layer.id, placement, extent);
    FocusHandleFocus(cx->win, state->focus);
    return true;
}

bool ShellRootCloseSheet(Ctx* cx) {
    if (!cx || !ShellRootOf(cx->win, cx->app)) return false;
    WindowLayers* layers = WindowLayersOf(cx->win);
    if (!layers || !layers->hasSheet) return false;
    ShellSheetLayer* state =
        Entity<ShellSheetLayer>{layers->sheet.view}.Get(cx);
    FocusHandle restore = state ? state->restore : FocusHandle{};
    WindowCloseSheet(cx);
    RestoreOverlayFocus(cx, restore);
    return true;
}

bool ShellRootHasSheet(Ctx* cx) {
    return cx && ShellRootOf(cx->win, cx->app) && WindowHasActiveSheet(cx);
}

static component::NotificationType NotificationTypeFor(ToastLevel level) {
    switch (level) {
        case ToastLevel::Info: return component::NotificationType::Info;
        case ToastLevel::Success: return component::NotificationType::Success;
        case ToastLevel::Warning: return component::NotificationType::Warning;
        case ToastLevel::Error: return component::NotificationType::Error;
    }
    return component::NotificationType::Info;
}

bool ShellRootPushToast(Ctx* cx, const ToastRequest& request) {
    ShellRoot* root = cx ? ShellRootOf(cx->win, cx->app) : nullptr;
    if (!root || !request.title) return false;
    Str id = request.id;
    if (!request.hasId) {
        root->nextToastOrdinal++;
        id = StrDup(cx->a,
                    fmt("shell-toast-%llu", root->nextToastOrdinal));
    }
    component::Notification toast = component::Notification::New();
    toast.Id1<ShellRoot>(id)
        .Title(request.title)
        .Message(request.description)
        .WithType(NotificationTypeFor(request.level));
    component::NotificationListState* list = WindowNotifications(cx).Get(cx);
    if (list) {
        list->width = 320;
        list->maxItems = 3;
    }
    return WindowPushNotification(cx, toast, request.timeoutMs) != 0;
}

bool ShellRootRemoveToast(Ctx* cx, Str id) {
    if (!cx || !id || !ShellRootOf(cx->win, cx->app)) return false;
    Entity<component::NotificationListState> handle = WindowNotifications(cx);
    component::NotificationListState* list = handle.Get(cx);
    uint32_t key = (uint32_t)HashClickId(id);
    bool found = false;
    for (int i = 0; i < list->items.len; i++) {
        const component::Notification& item = list->items[i];
        if (item.identityType == component::NotificationTypeOf<ShellRoot>() &&
            item.identityHasKey && item.identityKey == key) {
            found = true;
            break;
        }
    }
    if (found)
        WindowRemoveNotification1<ShellRoot>(cx, key);
    return found;
}

void ShellRootClearToasts(Ctx* cx) {
    if (cx && ShellRootOf(cx->win, cx->app)) WindowClearNotifications(cx);
}

int ShellRootToastCount(Ctx* cx) {
    return cx && ShellRootOf(cx->win, cx->app)
               ? WindowNotificationCount(cx)
               : 0;
}

} // namespace gpui
