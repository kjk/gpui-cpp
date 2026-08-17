#include "component/Popover.h"

namespace component {

Popover* Popover::New(Arena* a) {
    Popover* p = ::New<Popover>(a);
    p->a = a;
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
Popover* Popover::Open(bool v) {
    open = v;
    return this;
}

El* Popover::IntoEl() {
    return ::Popover::New(a, StrL("popover"))->Trigger(trigger)->Content(open ? content : nullptr)->IntoEl();
}

} // namespace component
