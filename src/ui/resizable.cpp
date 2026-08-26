#include "ui/resizable.h"

namespace gpui {

namespace component {

gpui::Resizable* Resizable::New(Ctx* cx, Str id, Entity<ResizableState> state,
                                Axis axis) {
    // The state is the base group's business, including keying its own.
    const Theme& th = ThemeNow(cx->app);
    return gpui::Resizable::New(cx, id, state, axis)
        ->HandleColors(th.border, th.dragBorder);
}

} // namespace component
} // namespace gpui
