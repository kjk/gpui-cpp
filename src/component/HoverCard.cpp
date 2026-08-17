#include "component/HoverCard.h"

namespace gpui {

namespace component {

HoverCard* HoverCard::New(Arena* a) {
    HoverCard* h = ArenaNew<HoverCard>(a);
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
    return gpui::HoverCard::New(a, StrL("hover-card"))
        ->Trigger(trigger)
        ->Content(open ? content : nullptr)
        ->IntoEl();
}

} // namespace component
} // namespace gpui
