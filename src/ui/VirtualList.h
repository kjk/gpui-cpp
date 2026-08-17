/* Unstyled virtual list host — crates/base/src/virtual_list.rs */

#pragma once

#include "gpui/Gpui.h"

namespace gpui {

struct VirtualList {
    static El* New(Arena* a, Str id);
};
} // namespace gpui
