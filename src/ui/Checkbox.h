/* Unstyled checkbox — crates/base/src/checkbox.rs */

#include "gpui/Gpui.h"

namespace gpui {

struct Checkbox {
    static El* New(Ctx* cx, Str id, int clickId = 0);
};

struct CheckboxIndicator {
    static El* New(Ctx* cx);
};
} // namespace gpui
