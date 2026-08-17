/* Unstyled checkbox — crates/base/src/checkbox.rs */

#pragma once

#include "gpui/Gpui.h"

struct Checkbox {
    static El* New(Arena* a, Str id, int clickId = 0);
};

struct CheckboxIndicator {
    static El* New(Arena* a);
};
