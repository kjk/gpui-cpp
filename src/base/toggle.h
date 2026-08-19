/* Unstyled toggle / toggle group — crates/base/src/toggle.rs */

#include "gpui/gpui.h"

namespace gpui {

struct Toggle {
    static El* New(Ctx* cx, Str id, int clickId = 0);
};

struct ToggleGroup {
    static El* New(Ctx* cx, Str id);
};
} // namespace gpui
