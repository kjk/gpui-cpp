/* Window border helper — crates/ui/src/window_border.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

struct WindowBorder {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    El* child = nullptr;

    static WindowBorder* New(Ctx* cx);
    WindowBorder* Child(El* e);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
