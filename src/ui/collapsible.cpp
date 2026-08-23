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

Collapsible* Collapsible::W(float v) {
    width = v;
    return this;
}
Collapsible* Collapsible::Gap(float v) {
    gap = v;
    return this;
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
    El* e = gpui::Collapsible::New(cx)
                ->FlexCol()
                ->Open(open)
                ->Child(trigger)
                ->Content(content)
                ->IntoEl();
    if (width != 0) {
        e->W(width);
    }
    if (gap != 0) {
        e->Gap(gap);
    }
    return e;
}

} // namespace component
} // namespace gpui
