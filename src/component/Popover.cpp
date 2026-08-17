#include "component/Popover.h"

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
Popover* Popover::Open(bool v) {
    open = v;
    return this;
}

El* Popover::IntoEl() {
    return gpui::Popover::New(cx, StrL("popover"))
        ->Trigger(trigger)
        ->Content(open ? content : nullptr)
        ->IntoEl();
}

} // namespace component
} // namespace gpui
