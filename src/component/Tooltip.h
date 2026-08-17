/* Themed tooltip — crates/ui/src/tooltip.rs */

#pragma once

#include "component/Common.h"

namespace gpui {

namespace component {

struct Tooltip {
    Arena* a = nullptr;
    Str text = {};

    static Tooltip* New(Arena* a, Str text);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
