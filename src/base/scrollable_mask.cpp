#include "base/scrollable_mask.h"

namespace gpui {

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

El* HorizontalScrollArea(Ctx* cx, Str id, El* viewport) {
    if (!viewport) {
        return Div(cx->a);
    }
    // Rust wraps the viewport in a relative div and puts the mask beside it,
    // because a child would be prepainted with the scroll offset applied and
    // slide away from the frame. The mask is the viewport here, so there is
    // nothing to slide: the axis is marked on the element that scrolls.
    ScrollableMask::Apply(viewport, Axis::Horizontal);
    // The mask's id only matters when it has to keep its gesture axis lock
    // apart from another mask's; a viewport that already names its own scroll
    // state has that identity already.
    if (id.s && !viewport->scrollId && !viewport->scrollFromPath) {
        viewport->PathId(id)->ScrollFromPath();
    }
    return viewport;
}

} // namespace gpui
