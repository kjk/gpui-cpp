/* Unstyled resizable — crates/base/src/resizable */

#pragma once

#include "gpui/Gpui.h"

struct Resizable {
    static El* New(Arena* a, Str id);
};
struct ResizablePanel {
    static El* New(Arena* a);
};
