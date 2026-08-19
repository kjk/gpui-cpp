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
Scrollable* Scrollable::ScrollX(float v) {
    scrollX = v;
    return this;
}
Scrollable* Scrollable::Axis(ScrollAxis v) {
    axis = v;
    return this;
}
Scrollable* Scrollable::Mode(ScrollbarMode v) {
    mode = v;
    return this;
}
Scrollable* Scrollable::H(float v) {
    h = v;
    return this;
}

El* Scrollable::IntoEl() {
    El* box = Scrollbar::New(cx)->H(h)->W(kFill)->ScrollMode(mode);
    // Each axis is asked for on its own: a box that only scrolls down still
    // clips what runs off its side.
    if (axis == ScrollAxis::Vertical || axis == ScrollAxis::Both) {
        box->ClipY()->ScrollY(scrollY);
    } else {
        box->ClipY();
    }
    if (axis == ScrollAxis::Horizontal || axis == ScrollAxis::Both) {
        box->ScrollX(scrollX);
    } else {
        box->ClipX();
    }
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
