/* Unstyled link — crates/base/src/link.rs
   href is target data. Navigation is application-owned (showcase logs the
   path). */

#pragma once

#include "gpui/Gpui.h"

namespace gpui {

struct Link {
    static El* New(Arena* a, Str id, int clickId = 0);
};
} // namespace gpui
