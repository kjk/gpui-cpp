/* Unstyled selectable text host — crates/base/src/text_selection.rs */

#pragma once

#include "gpui/Gpui.h"

struct TextSelection {
    static El* New(Arena* a, Str id, int clickId = 0);
};
