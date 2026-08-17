#include "component/HoverCard.h"

namespace component {

HoverCard* HoverCard::New(Arena* a) {
    HoverCard* h = ::New<HoverCard>(a);
    h->a = a;
    return h;
}
HoverCard* HoverCard::Trigger(El* e) {
    trigger = e;
    return this;
}
HoverCard* HoverCard::Content(El* e) {
    content = e;
    return this;
}
HoverCard* HoverCard::Open(bool v) {
    open = v;
    return this;
}

El* HoverCard::IntoEl() {
    return ::HoverCard::New(a, StrL("hover-card"))
        ->Trigger(trigger)
        ->Content(open ? content : nullptr)
        ->IntoEl();
}

} // namespace component
