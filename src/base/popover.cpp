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

// toggle_open's focus half, which is the same for every widget that opens
// something over the page. On the way in the previously focused element is
// parked and the popover — or whatever it tracks — takes focus; on the way
// out focus goes back, but only if the popover still has it, since a click
// somewhere else has already moved it on purpose.
void PopoverSetOpenFocused(PopoverState* s, Ctx* cx, bool open) {
    if (!s || s->open == open) {
        return;
    }
    if (open) {
        s->previousFocusId = WindowFocusedId(cx->win);
        int take = s->trackedFocusId ? s->trackedFocusId : s->focusId;
        if (take) {
            WindowSetFocusId(cx->win, take);
        }
    } else {
        if (s->previousFocusId &&
            (WindowFocusWithin(cx->win, s->focusId) ||
             WindowFocusWithin(cx->win, s->trackedFocusId))) {
            WindowRestoreFocus(cx->win, s->previousFocusId);
        }
        s->previousFocusId = 0;
    }
    s->open = open;
}

// toggle_open, off the trigger's press. Rust stops propagation here so the
// press does not also reach whatever the popover sits in; the hit test only
// reports the innermost rect, so that is already true.
void PopoverToggle(PopoverState* self, Ctx* cx, const MouseDownEvent* ev,
                   intptr_t button) {
    if (ev->button != (MouseButton)button) {
        return;
    }
    PopoverSetOpenFocused(self, cx, !self->open);
    Notify(cx);
}

void PopoverDismiss(PopoverState* self, Ctx* cx, const ClickEvent*) {
    if (!self->open) {
        return;
    }
    PopoverSetOpenFocused(self, cx, false);
    Notify(cx);
}

Popover* Popover::New(Ctx* cx, Str id, Entity<PopoverState> state,
                      MouseButton button) {
    Arena* a = cx->a;
    Popover* p = ArenaNew<Popover>(a);
    p->a = a;
    p->cx = cx;
    p->state = state;
    p->button = button;
    p->root = Div(a)->Id(id);
    // The popover's own focus handle. Rust hangs it off the content, not off
    // the trigger's container, so opening moves focus into what came up and
    // Tab from the trigger still walks the page.
    p->focusId = HashClickId(id) * 31 + 1;
    if (PopoverState* st = state.Get(cx)) {
        st->focusId = p->focusId;
    }
    return p;
}

Popover* Popover::TrackedFocus(int trackedId) {
    if (PopoverState* st = state.Get(cx)) {
        st->trackedFocusId = trackedId;
    }
    return this;
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

Popover* Popover::Anchor(PopupAnchor v) {
    anchor = v;
    return this;
}

Popover* Popover::Content(El* content) {
    if (content) {
        // What `Popup` does with it, because Rust's Popover *is* a Popup:
        // `Popup::new(id, trigger).content(..)`. Four pixels under the
        // trigger — the `top_1` on `render_popover_content` — and deferred,
        // so it draws over whatever follows the trigger in the tree and is
        // not cut by an ancestor's overflow. The port had it absolute at a
        // hard-coded `top: 28`, in flow, and so clipped by the section it
        // was in and mispositioned under any trigger that was not 28 tall.
        // Where it hangs, which is what `Positioner::corner` works out.
        PopupPlaceContent(content, anchor, 4);
        // track_focus, not focus_ring_style: the surface takes focus and
        // does not draw a ring around itself for it.
        content->FocusId(focusId)->FocusRing(false);
        root->Child(content);
    }
    return this;
}

El* Popover::IntoEl() {
    return root;
}
} // namespace gpui
