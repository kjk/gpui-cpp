/* Themed color picker — crates/ui/src/color_picker.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

struct ColorPicker {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    uint32_t hex = 0x2563eb;
    bool open = false;
    Listener onChange;
    Listener onToggle;

    static ColorPicker* New(Ctx* cx);
    ColorPicker* Hex(uint32_t h);
    ColorPicker* Open(bool v);
    ColorPicker* OnChange(Listener fn);
    ColorPicker* OnToggle(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
