#include "ui/resizable.h"

namespace gpui {

namespace component {

gpui::Resizable* Resizable::New(Ctx* cx, Str id, Entity<ResizableState> state,
                                Axis axis) {
    const Theme& th = cx->theme();
    return gpui::Resizable::New(cx, id, state, axis)
        ->HandleColors(th.border, th.dragBorder);
}

} // namespace component
} // namespace gpui
