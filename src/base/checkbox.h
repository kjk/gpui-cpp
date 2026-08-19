/* Unstyled checkbox — crates/base/src/checkbox.rs */

#include "gpui/gpui.h"

namespace gpui {

struct Checkbox {
    static El* New(Ctx* cx, Str id, int clickId = 0);
};

struct CheckboxIndicator {
    static El* New(Ctx* cx);
};
} // namespace gpui
