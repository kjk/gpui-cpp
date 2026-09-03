#include "shell/view.h"
#include "base/resizable.h"
#include "base/select.h"

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
    uint32_t revision = shell::ThemeTokensSync(cx->app);
    if (revision != self->themeRevision) {
        self->themeRevision = revision;
        self->dirty = true;
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
            // Measured here rather than anywhere else because this is the only
            // place two consecutive descriptions of one view exist at the same
            // time. Nothing acts on the answer: it counts how often a rebuild
            // produced the shape it replaced, which is what a template cache
            // would have to be able to fill instead of rebuild. A first build
            // has no predecessor and is not a data point either way.
            if (self->snapshot) {
                self->runtime->RecordStructure(self->snapshot->Structure() ==
                                               next->Structure());
            }
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

void ScriptView::OnResize(ScriptView* self, Ctx* cx,
                          const ResizablePanelEvent* event,
                          intptr_t callback) {
    if (!self || !self->runtime || !event) return;
    self->runtime->DispatchNumbers((shell::CallbackId)callback, event->sizes,
                                   event->count, cx->win, cx->app);
}

void ScriptView::OnBoundBool(ScriptView* self, Ctx* cx, const void*,
                             intptr_t binding) {
    ShellBoolBinding* value = (ShellBoolBinding*)binding;
    if (!self || !self->runtime || !value || !value->callback) return;
    self->runtime->DispatchChange(value->callback, value->value, cx->win,
                                  cx->app);
}

void ScriptView::OnBoundString(ScriptView* self, Ctx* cx,
                               const ClickEvent*, intptr_t binding) {
    ShellStringBinding* value = (ShellStringBinding*)binding;
    if (!self || !self->runtime || !value || !value->callback) return;
    self->runtime->DispatchString(value->callback, value->value, cx->win,
                                  cx->app);
}

// Only the secondary button: the row already reports its click through
// on_item_click, and a left press here would report the same interaction twice.
void ScriptView::OnItemSecondaryPress(ScriptView* self, Ctx* cx,
                                      const MouseDownEvent* event,
                                      intptr_t binding) {
    ShellStringBinding* value = (ShellStringBinding*)binding;
    if (!self || !self->runtime || !event || !value || !value->callback) return;
    if (event->button != MouseButton::Right) return;
    self->runtime->DispatchItemSecondaryClick(value->callback, value->value,
                                              *event, cx->win, cx->app);
}

void ScriptView::OnSelectAction(ScriptView* self, Ctx* cx,
                                const ActionEvent* event,
                                intptr_t binding) {
    ShellSelectBinding* value = (ShellSelectBinding*)binding;
    if (!self || !self->runtime || !event || !value) return;
    switch (SelectActionOf(event->action, value->open, value->disabled)) {
        case SelectAction::Open:
            if (value->contentFocus.IsValid())
                FocusHandleFocus(cx->win, value->contentFocus);
            if (value->onOpenChange)
                self->runtime->DispatchChange(value->onOpenChange, true,
                                              cx->win, cx->app);
            break;
        case SelectAction::Confirm:
            if (value->onConfirm)
                self->runtime->DispatchSignal(value->onConfirm, cx->win,
                                              cx->app);
            break;
        case SelectAction::Dismiss:
            if (value->onDismiss)
                self->runtime->DispatchSignal(value->onDismiss, cx->win,
                                              cx->app);
            if (value->triggerFocus.IsValid())
                FocusHandleFocus(cx->win, value->triggerFocus);
            if (value->onOpenChange)
                self->runtime->DispatchChange(value->onOpenChange, false,
                                              cx->win, cx->app);
            break;
        case SelectAction::None:
            const_cast<ActionEvent*>(event)->propagate = true;
            break;
    }
}

void ScriptView::OnSelectOpen(ScriptView* self, Ctx* cx,
                              const ClickEvent*, intptr_t binding) {
    ShellSelectBinding* value = (ShellSelectBinding*)binding;
    if (!self || !self->runtime || !value || value->disabled || value->open)
        return;
    if (value->contentFocus.IsValid())
        FocusHandleFocus(cx->win, value->contentFocus);
    if (value->onOpenChange)
        self->runtime->DispatchChange(value->onOpenChange, true, cx->win,
                                      cx->app);
}

void ScriptView::OnNumberStep(ScriptView* self, Ctx* cx,
                              const NumberInputEvent* event,
                              intptr_t callback) {
    if (!self || !self->runtime || !event || !callback) return;
    self->runtime->DispatchString(
        (shell::CallbackId)callback,
        event->action == StepAction::Increment ? StrL("increment")
                                               : StrL("decrement"),
        cx->win, cx->app);
}

void ScriptView::OnNumberKey(ScriptView* self, Ctx* cx,
                             const KeyEvent* event, intptr_t binding) {
    ShellNumberBinding* value = (ShellNumberBinding*)binding;
    if (!self || !event || !value) return;
    StepAction action;
    if (!NumberStepForKey(event->vk, &action)) return;
    Listener onStep = value->onStep
                          ? Listen(cx, &ScriptView::OnNumberStep,
                                   (intptr_t)value->onStep)
                          : Listener{};
    const NumberStep* step = value->onStep || !value->hasStep
                                 ? nullptr
                                 : &value->step;
    if (NumberInputApplyStep(value->state, cx->app, cx->win, action, step,
                             value->hasMin, value->min, value->hasMax,
                             value->max, value->disabled, onStep)) {
        const_cast<KeyEvent*>(event)->propagate = false;
        Notify(cx);
    }
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
