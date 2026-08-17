/* Unstyled tooltip popup — crates/base/src/tooltip.rs */

#pragma once

#include "gpui/Gpui.h"

namespace gpui {

struct Tooltip {
    static El* New(Arena* a, Str id);
};
} // namespace gpui
