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
    modeSet = true;
    return this;
}
Scrollable* Scrollable::H(float v) {
    h = v;
    return this;
}

El* Scrollable::IntoEl() {
    // The scrolled box is the base one — the clip, the offsets, the id and
    // the listener all come together there, so the themed wrapper cannot end
    // up with a box that clips but does not take the wheel.
    El* box = Scrollbar::New(cx, id.s ? id : StrL("scrollable"), scrollY,
                             scrollX, onScroll, axis,
                             modeSet ? mode : ScrollbarModeNow(cx->app))
                  ->H(h)
                  ->W(kFill);
    if (child) {
        box->Child(child);
    }
    return box;
}

} // namespace component
} // namespace gpui
