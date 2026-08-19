#include "ui/collapsible.h"

namespace gpui {

namespace component {

Collapsible* Collapsible::New(Ctx* cx) {
    Arena* a = cx->a;
    Collapsible* c = ArenaNew<Collapsible>(a);
    c->a = a;
    c->cx = cx;
    return c;
}

Collapsible* Collapsible::Open(bool v) {
    open = v;
    return this;
}
Collapsible* Collapsible::Trigger(El* e) {
    trigger = e;
    return this;
}
Collapsible* Collapsible::Content(El* e) {
    content = e;
    return this;
}

El* Collapsible::IntoEl() {
    // The unstyled root has no direction of its own, as Rust's plain div()
    // does not; a collapsible stacks its trigger over its content.
    return gpui::Collapsible::New(cx)
        ->FlexCol()
        ->Open(open)
        ->Child(trigger)
        ->Content(content)
        ->IntoEl();
}

} // namespace component
} // namespace gpui
