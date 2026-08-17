/* Themed color picker — crates/ui/src/color_picker.rs */

#pragma once

#include "component/Common.h"

namespace gpui {

namespace component {

struct ColorPicker {
    Arena* a = nullptr;
    u32 hex = 0x2563eb;
    bool open = false;
    Func1<u32> onChange;
    Func0 onToggle;

    static ColorPicker* New(Arena* a);
    ColorPicker* Hex(u32 h);
    ColorPicker* Open(bool v);
    ColorPicker* OnChange(Func1<u32> fn);
    ColorPicker* OnToggle(Func0 fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
