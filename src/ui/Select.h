/* Unstyled select — crates/base/src/select.rs */

#pragma once

#include "gpui/Gpui.h"

namespace gpui {

struct Select {
    static El* New(Arena* a, Str id);
};
} // namespace gpui
