/* Unstyled color picker — crates/base/src/color_picker.rs */

#pragma once

#include "gpui/Gpui.h"

namespace gpui {

struct ColorPicker {
    static El* New(Arena* a, Str id);
};
struct ColorSwatch {
    static El* New(Arena* a, Str id, int clickId = 0);
};
} // namespace gpui
