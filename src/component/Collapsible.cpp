#include "component/Collapsible.h"

namespace gpui {

namespace component {

Collapsible* Collapsible::New(Arena* a) {
    Collapsible* c = ArenaNew<Collapsible>(a);
    c->a = a;
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
    return gpui::Collapsible::New(a)
        ->Open(open)
        ->Child(trigger)
        ->Content(content)
        ->IntoEl();
}

} // namespace component
} // namespace gpui
