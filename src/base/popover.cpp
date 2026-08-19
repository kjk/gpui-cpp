#include "base/popover.h"

namespace gpui {

bool PopoverIsOpen(Ctx* cx, Entity<PopoverState> state) {
    PopoverState* s = state.Get(cx);
    return s && s->open;
}

void PopoverSetOpen(Ctx* cx, Entity<PopoverState> state, bool open) {
    PopoverState* s = state.Get(cx);
    if (s) {
        s->open = open;
    }
}

// toggle_open, off the trigger's press. Rust stops propagation here so the
// press does not also reach whatever the popover sits in; the hit test only
// reports the innermost rect, so that is already true.
void PopoverToggle(PopoverState* self, Ctx* cx, const MouseDownEvent* ev,
                   intptr_t button) {
    if (ev->button != (MouseButton)button) {
        return;
    }
    self->open = !self->open;
    Notify(cx);
}

void PopoverDismiss(PopoverState* self, Ctx* cx, const ClickEvent*) {
    if (!self->open) {
        return;
    }
    self->open = false;
    Notify(cx);
}

Popover* Popover::New(Ctx* cx, Str id, Entity<PopoverState> state,
                      MouseButton button) {
    Arena* a = cx->a;
    Popover* p = ArenaNew<Popover>(a);
    p->a = a;
    p->state = state;
    p->button = button;
    p->root = Div(a)->Id(id);
    return p;
}

Popover* Popover::Trigger(El* trigger) {
    if (!trigger) {
        return this;
    }
    if (state.IsValid()) {
        // A press, not a click: Rust hangs the toggle off on_mouse_down so the
        // popover is up before the button comes back. The handler reads the
        // event's own button, since one element hears every press it is over.
        trigger->OnMouseDown(ListenTo(state, &PopoverToggle, (intptr_t)button));
    }
    root->Child(trigger);
    return this;
}

Popover* Popover::Content(El* content) {
    if (content) {
        content->Absolute()->Top(28)->Left(0);
        root->Child(content);
    }
    return this;
}

El* Popover::IntoEl() {
    return root;
}
} // namespace gpui
