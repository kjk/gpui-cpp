/* Window border helper — crates/ui/src/window_border.rs */

#pragma once

#include "component/Common.h"

namespace gpui {

namespace component {

struct WindowBorder {
    Arena* a = nullptr;
    El* child = nullptr;

    static WindowBorder* New(Arena* a);
    WindowBorder* Child(El* e);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
