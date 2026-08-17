/* Themed color picker — crates/ui/src/color_picker.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

struct ColorPicker {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    uint32_t hex = 0x2563eb;
    bool open = false;
    Func1<uint32_t> onChange;
    Func0 onToggle;

    static ColorPicker* New(Ctx* cx);
    ColorPicker* Hex(uint32_t h);
    ColorPicker* Open(bool v);
    ColorPicker* OnChange(Func1<uint32_t> fn);
    ColorPicker* OnToggle(Func0 fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
