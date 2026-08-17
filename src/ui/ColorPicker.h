/* Unstyled color picker — crates/base/src/color_picker.rs */

#include "gpui/Gpui.h"

namespace gpui {

struct ColorPicker {
    static El* New(Ctx* cx, Str id);
};
struct ColorSwatch {
    static El* New(Ctx* cx, Str id, int clickId = 0);
};
} // namespace gpui
