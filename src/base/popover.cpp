#include "base/popover.h"
#include "base/global_state.h"

namespace gpui {

bool PopoverIsOpen(Ctx* cx, Entity<PopoverState> state) {
    PopoverState* s = state.Get(cx);
    return s && s->open;
}

void PopoverSetOpen(Ctx* cx, Entity<PopoverState> state, bool open) {
    PopoverState* s = state.Get(cx);
    if (s) {
        s->self = state.id;
        s->open = open;
        BaseDeferredPopoverSet(cx->app, state.id, open);
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
        s->previousFocus = WindowFocused(cx->win);
        FocusHandle take =
            s->trackedFocus.IsValid() ? s->trackedFocus : s->focus;
        FocusHandleFocus(cx->win, take);
    } else {
        if (s->previousFocus.IsValid() &&
            (FocusHandleContainsFocused(cx->win, s->focus) ||
             FocusHandleContainsFocused(cx->win, s->trackedFocus))) {
            FocusHandleRestore(cx->win, s->previousFocus);
        }
        s->previousFocus = {};
    }
    s->open = open;
    BaseDeferredPopoverSet(cx->app,
                           s->self.IsValid() ? s->self : cx->self, open);
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
    p->id = id;
    p->state = state;
    p->button = button;
    // The popover's own focus handle. Rust hangs it off the content, not off
    // the trigger's container, so opening moves focus into what came up and
    // Tab from the trigger still walks the page.
    // `PopoverState::new` asks the app for a handle once and keeps it; the
    // state outlives the frame, so the handle it holds is what the content
    // picks up again each time. Nothing about it comes from `id` any more —
    // the old `HashClickId(id) * 31 + 1` existed only to stay clear of the
    // click id that same name produced.
    if (PopoverState* st = state.Get(cx)) {
        st->self = state.id;
        if (st->open) {
            BaseDeferredPopoverSet(cx->app, state.id, true);
        }
        if (!st->focus.IsValid()) {
            st->focus = FocusHandleNew(cx);
        }
        p->focus = st->focus;
    }
    return p;
}

Popover* Popover::TrackedFocus(FocusHandle tracked) {
    if (PopoverState* st = state.Get(cx)) {
        st->trackedFocus = tracked;
    }
    return this;
}

Popover* Popover::Trigger(El* e) {
    if (!e) {
        return this;
    }
    if (state.IsValid()) {
        // A press, not a click: Rust hangs the toggle off on_mouse_down so the
        // popover is up before the button comes back. The handler reads the
        // event's own button, since one element hears every press it is over.
        e->OnMouseDown(ListenTo(state, &PopoverToggle, (intptr_t)button));
    }
    trigger = e;
    return this;
}

Popover* Popover::Anchor(PopupAnchor v) {
    anchor = v;
    return this;
}

Popover* Popover::Content(El* e) {
    if (e) {
        // What `Popup` does with it, because Rust's Popover *is* a Popup:
        // `Popup::new(id, trigger).content(..)`. The exact requested corner
        // is deferred, so it draws over later siblings and is not clipped by
        // an ancestor's overflow. The styled UI layer supplies its own
        // top_1/bottom_1 visual offset.
        e->Role(AccessibilityRole::Dialog);
        // track_focus, not focus_ring_style: the surface takes focus and
        // does not draw a ring around itself for it.
        e->TrackFocus(focus)->FocusRing(false);
        content = e;
    }
    return this;
}

El* Popover::IntoEl() {
    if (!trigger) {
        return Div(a)->Id(StrL("empty"));
    }
    // popover.rs builds Popup::new(id, trigger).anchor(anchor), rather than
    // duplicating its capture and positioning lifecycle.
    return Popup::New(cx, id, trigger, anchor)->Content(content)->IntoEl();
}
} // namespace gpui
