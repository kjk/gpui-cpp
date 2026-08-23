#include "ui/popover.h"
#include "base/actions.h"

namespace gpui {

namespace component {

Popover* Popover::New(Ctx* cx) {
    Arena* a = cx->a;
    Popover* p = ArenaNew<Popover>(a);
    p->a = a;
    p->cx = cx;
    return p;
}
Popover* Popover::Trigger(El* e) {
    trigger = e;
    return this;
}
Popover* Popover::Content(El* e) {
    content = e;
    return this;
}
Popover* Popover::New(Ctx* cx, Str id) {
    Popover* p = New(cx);
    p->id = id;
    return p;
}
Popover* Popover::Open(bool v) {
    controlled = true;
    open = v;
    return this;
}
Popover* Popover::DefaultOpen(bool v) {
    defaultOpen = v;
    return this;
}
Popover* Popover::OnClose(Listener fn) {
    onClose = fn;
    return this;
}
Popover* Popover::Anchor(PopupAnchor v) {
    anchor = v;
    return this;
}
Popover* Popover::Button(MouseButton b) {
    button = b;
    return this;
}

// The keyed state behind one popover id — Rust's
// `window.use_keyed_state(self.id, |cx| PopoverState::new(default_open, cx))`.
static Entity<PopoverState> PopState(Ctx* cx, Str id) {
    return KeyedEntity<PopoverState>(cx, (uint32_t)HashClickId(id));
}

bool PopoverOpen(Ctx* cx, Str id) {
    return PopoverIsOpen(cx, PopState(cx, id));
}

El* Popover::IntoEl() {
    Str popId = id.s ? id : StrL("popover");
    Entity<PopoverState> st = PopState(cx, popId);
    PopoverState* s = st.Get(cx);
    // default_open only counts the first time this key is seen, the way it
    // only reaches PopoverState::new once.
    if (s && !s->seeded) {
        s->seeded = true;
        s->open = defaultOpen;
    }
    if (controlled) {
        PopoverSetOpen(cx, st, open);
    }
    bool isOpen = PopoverIsOpen(cx, st);
    El* root = gpui::Popover::New(cx, popId, st, button)
                   ->Anchor(anchor)
                   ->Trigger(trigger)
                   ->Content(isOpen ? content : nullptr)
                   ->IntoEl();
    // popover.rs binds escape to Cancel in the "Popover" context and closes
    // on it. A controlled popover's flag is the caller's, so it says what to
    // run; an uncontrolled one closes its own state.
    if (isOpen) {
        Listener close =
            onClose.IsValid() ? onClose : ListenTo(st, &PopoverDismiss);
        CancelBindKeys(cx, root, "Popover", popId, close);
    }
    return root;
}

} // namespace component
} // namespace gpui
