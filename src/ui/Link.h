/* Unstyled link — crates/base/src/link.rs
   href is target data. Navigation is application-owned (showcase logs the path). */

#pragma once

#include "gpui/Gpui.h"

struct Link {
    static El* New(Arena* a, Str id, int clickId = 0);
};
