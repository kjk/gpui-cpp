#include "component/Collapsible.h"

namespace component {

Collapsible* Collapsible::New(Arena* a) {
    Collapsible* c = ::New<Collapsible>(a);
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
    return ::Collapsible::New(a)
        ->Open(open)
        ->Child(trigger)
        ->Content(content)
        ->IntoEl();
}

} // namespace component
