#include "shell/view.h"

namespace gpui {

ScriptView::~ScriptView() {
    if (runtime && self.IsValid()) {
        runtime->UnregisterScriptView(self, &dirty);
        runtime->ReleaseOwnedEntities(self);
    }
    delete snapshot;
    ViewObjectRelease(object);
    ViewTypeRelease(type);
    PolicyRelease(policy);
    ShellErrorClear(&error);
    if (runtime) runtime->Release();
}

Entity<ScriptView> ScriptView::New(App* app, ShellRuntime* runtime,
                                   ViewType* type, Policy* policy) {
    Entity<ScriptView> entity = EntityNew<ScriptView>(app);
    ScriptView* view = entity.Get(app);
    if (!view) return {};
    view->runtime = runtime ? runtime->Retain() : nullptr;
    view->type = ViewTypeRetain(type);
    view->policy = policy ? PolicyRetain(policy) : PolicyDefault();
    view->self = entity.id;
    if (view->runtime) view->runtime->RegisterScriptView(entity.id, &view->dirty);
    return entity;
}

El* ScriptView::Render(ScriptView* self, Ctx* cx) {
    if (!self || !self->runtime || !self->type) {
        return Div(cx->a)->Child(TextEl(cx->a, StrL("Shell view is not initialized")));
    }
    if (!self->object) {
        self->object = self->runtime->Instantiate(
            self->type, cx->win, cx->app, self->policy, &self->error, cx->self);
    }
    if (self->object && (self->dirty || !self->snapshot)) {
        RenderSnapshot* next = self->runtime->BuildSnapshot(
            self->object, cx->win, cx->app, cx->self, self->policy,
            &self->error);
        if (next) {
            delete self->snapshot;
            self->snapshot = next;
            self->dirty = false;
            ShellErrorClear(&self->error);
        }
    }
    if (self->snapshot) {
        return ShellMaterialize(cx, self->runtime, self->snapshot, &self->error);
    }
    Str message = self->error.IsSet() ? self->error.message
                                      : StrL("The shell view did not publish a snapshot");
    return Div(cx->a)
        ->FlexCol()
        ->SizeFull()
        ->Pad(16)
        ->Gap(8)
        ->Child(TextEl(cx->a, StrL("JavaScript application error"))->Bold())
        ->Child(TextEl(cx->a, message)->Wrap());
}

void ScriptView::Refresh(ScriptView* self, Ctx* cx) {
    if (!self) return;
    self->dirty = true;
    Notify(cx);
}

bool ScriptView::Reload(ScriptView* self, Ctx* cx, Str directory, Str entry,
                        ShellError* error) {
    ShellErrorClear(error);
    if (!self || !self->runtime || !cx || cx->self != self->self) {
        ShellErrorSet(error, StrL("reload needs the live ScriptView context"));
        return false;
    }
    ViewType* nextType =
        self->runtime->LoadApp(directory, entry, self->policy, error);
    if (!nextType) return false;
    ViewObject* nextObject = self->runtime->Instantiate(
        nextType, cx->win, cx->app, self->policy, error, cx->self);
    if (!nextObject) {
        ViewTypeRelease(nextType);
        return false;
    }

    ViewObject* oldObject = self->object;
    ViewType* oldType = self->type;
    RenderSnapshot* oldSnapshot = self->snapshot;
    self->object = nextObject;
    self->type = nextType;
    self->snapshot = nullptr;
    self->dirty = true;
    ShellErrorClear(&self->error);
    if (oldObject) self->runtime->ReleaseApplicationState(oldObject);
    delete oldSnapshot;
    ViewObjectRelease(oldObject);
    ViewTypeRelease(oldType);
    Notify(cx);
    return true;
}

void ScriptView::OnClick(ScriptView* self, Ctx* cx,
                         const ClickEvent* event, intptr_t callback) {
    if (!self || !self->runtime || !event) return;
    self->runtime->DispatchClick((shell::CallbackId)callback, *event, cx->win,
                                 cx->app);
}

void ScriptView::OnChange(ScriptView* self, Ctx* cx,
                          const ClickEvent* event, intptr_t value) {
    if (!self || !self->runtime || !event || event->id <= 0) return;
    self->runtime->DispatchChange((shell::CallbackId)(uint32_t)event->id,
                                  value != 0, cx->win, cx->app);
}

void ScriptView::OnHover(ScriptView* self, Ctx* cx,
                         const HoverEvent* event, intptr_t callback) {
    if (!self || !self->runtime || !event) return;
    self->runtime->DispatchChange((shell::CallbackId)callback, event->hovered,
                                  cx->win, cx->app);
}

void ScriptView::OnMouseMove(ScriptView* self, Ctx* cx,
                             const MouseMoveEvent* event,
                             intptr_t callback) {
    if (!self || !self->runtime || !event) return;
    self->runtime->DispatchMouseMove((shell::CallbackId)callback, *event,
                                    cx->win, cx->app);
}

void ScriptView::OnOpenChange(ScriptView* self, Ctx* cx,
                              const PopoverOpenChangeEvent* event,
                              intptr_t callback) {
    if (!self || !self->runtime || !event) return;
    self->runtime->DispatchChange((shell::CallbackId)callback, event->open,
                                  cx->win, cx->app);
}

void ScriptView::OnInputEvent(ScriptView* self, Ctx* cx,
                              const InputEvent* event, intptr_t handle) {
    if (!self || !self->runtime || !event) return;
    self->runtime->DispatchInputEvent((shell::EntityHandle)handle, *event,
                                      cx->win, cx->app);
}

void ScriptView::OnSliderEvent(ScriptView* self, Ctx* cx,
                               const SliderEvent* event, intptr_t handle) {
    if (!self || !self->runtime || !event) return;
    self->runtime->DispatchSliderEvent((shell::EntityHandle)handle, *event,
                                       cx->win, cx->app);
}

void ScriptView::OnOtpEvent(ScriptView* self, Ctx* cx, const OtpEvent* event,
                            intptr_t handle) {
    if (!self || !self->runtime || !event) return;
    self->runtime->DispatchOtpEvent((shell::EntityHandle)handle, *event,
                                    cx->win, cx->app);
}

} // namespace gpui
