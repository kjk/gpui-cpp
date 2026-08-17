/* Unstyled color picker — crates/base/src/color_picker.rs */

#pragma once

#include "gpui/Gpui.h"

struct ColorPicker {
    static El* New(Arena* a, Str id);
};
struct ColorSwatch {
    static El* New(Arena* a, Str id, int clickId = 0);
};
