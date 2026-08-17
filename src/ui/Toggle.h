/* Unstyled toggle / toggle group — crates/base/src/toggle.rs */

#include "gpui/Gpui.h"

namespace gpui {

struct Toggle {
    static El* New(Ctx* cx, Str id, int clickId = 0);
};

struct ToggleGroup {
    static El* New(Ctx* cx, Str id);
};
} // namespace gpui
