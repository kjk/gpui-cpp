#include "ui/scroll.h"

namespace gpui {

namespace component {

Scrollable* Scrollable::New(Ctx* cx) {
    Arena* a = cx->a;
    Scrollable* s = ArenaNew<Scrollable>(a);
    s->a = a;
    s->cx = cx;
    s->element = Div(a)->W(kFill);
    return s;
}
Scrollable* Scrollable::New(Ctx* cx, Str id) {
    Scrollable* s = New(cx);
    s->id = id;
    return s;
}
Scrollable* Scrollable::New(Ctx* cx, El* element, ScrollAxis axis) {
    Scrollable* s = New(cx);
    s->element = element ? element : Div(cx->a);
    s->axis = axis;
    return s;
}
Scrollable* Scrollable::Id(Str v) {
    id = v;
    return this;
}
Scrollable* Scrollable::OnScroll(Listener fn) {
    onScroll = fn;
    return this;
}
Scrollable* Scrollable::Child(El* e) {
    if (element && e) {
        element->Child(e);
    }
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
    hSet = true;
    return this;
}

El* Scrollable::IntoEl() {
    // The scrolled box is the base one — the clip, the offsets, the id and
    // the listener all come together there, so the themed wrapper cannot end
    // up with a box that clips but does not take the wheel.
    El* box = Scrollbar::Apply(cx, element, id.s ? id : StrL("scrollable"),
                               scrollY, scrollX, onScroll, axis,
                               modeSet ? mode : ScrollbarModeNow(cx->app));
    if (hSet) {
        box->H(h);
    }
    return box;
}

El* ScrollableElement::Scrollbar(Ctx* cx, El* element, Str id, float scrollY,
                                 float scrollX, Listener onScroll,
                                 ScrollbarAxis axis) {
    return gpui::Scrollbar::Apply(cx, element, id, scrollY, scrollX, onScroll,
                                  axis);
}

El* ScrollableElement::VerticalScrollbar(Ctx* cx, El* element, Str id,
                                         float scrollY, Listener onScroll) {
    return Scrollbar(cx, element, id, scrollY, 0, onScroll,
                     ScrollbarAxis::Vertical);
}

El* ScrollableElement::HorizontalScrollbar(Ctx* cx, El* element, Str id,
                                           float scrollX,
                                           Listener onScroll) {
    return Scrollbar(cx, element, id, 0, scrollX, onScroll,
                     ScrollbarAxis::Horizontal);
}

Scrollable* ScrollableElement::OverflowScrollbar(Ctx* cx, El* element) {
    return Scrollable::New(cx, element, ScrollAxis::Both);
}

Scrollable* ScrollableElement::OverflowXScrollbar(Ctx* cx, El* element) {
    return Scrollable::New(cx, element, ScrollAxis::Horizontal);
}

Scrollable* ScrollableElement::OverflowYScrollbar(Ctx* cx, El* element) {
    return Scrollable::New(cx, element, ScrollAxis::Vertical);
}

ScrollableMask* ScrollableMask::New(Ctx* cx, Axis axis, El* element) {
    ScrollableMask* mask = ArenaNew<ScrollableMask>(cx->a);
    mask->a = cx->a;
    mask->axis = axis;
    mask->element = element;
    return mask;
}

El* ScrollableMask::Apply(El* element, Axis axis) {
    if (!element) {
        return nullptr;
    }
    element->ScrollMask(axis);
    if (!element->scrollId && !element->scrollFromPath) {
        element->ScrollFromPath();
    }
    return element;
}

ScrollableMask* ScrollableMask::Id(Str v) {
    id = v;
    return this;
}

ScrollableMask* ScrollableMask::Debug(bool v) {
    debug = v;
    return this;
}

El* ScrollableMask::IntoEl() {
    El* result = Apply(element, axis);
    if (!result) {
        return Div(a);
    }
    if (id.s) {
        result->PathId(id)->ScrollFromPath();
    }
    if (debug) {
        result->Border(1, Rgb(0xff, 0xff, 0));
    }
    return result;
}

} // namespace component
} // namespace gpui
