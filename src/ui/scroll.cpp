#include "ui/scroll.h"

namespace gpui {

namespace component {

Scrollable* Scrollable::New(Ctx* cx) {
    Arena* a = cx->a;
    Scrollable* s = ArenaNew<Scrollable>(a);
    s->a = a;
    s->cx = cx;
    return s;
}
Scrollable* Scrollable::New(Ctx* cx, Str id) {
    Scrollable* s = New(cx);
    s->id = id;
    return s;
}
Scrollable* Scrollable::OnScroll(Listener fn) {
    onScroll = fn;
    return this;
}
Scrollable* Scrollable::Child(El* e) {
    child = e;
    return this;
}
Scrollable* Scrollable::ScrollY(float v) {
    scrollY = v;
    return this;
}
Scrollable* Scrollable::H(float v) {
    h = v;
    return this;
}

El* Scrollable::IntoEl() {
    El* box = Scrollbar::New(cx)->H(h)->ClipY()->ScrollY(scrollY)->W(kFill);
    if (onScroll.IsValid()) {
        box->ScrollId(HashClickId(id.s ? id : StrL("scrollable")))
            ->OnScroll(onScroll);
    }
    if (child) {
        box->Child(child);
    }
    return box;
}

} // namespace component
} // namespace gpui
