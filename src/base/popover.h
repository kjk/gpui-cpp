/* Unstyled popover — crates/base/src/popover.rs */

#include "gpui/gpui.h"

namespace gpui {

struct Popover {
    El* root = nullptr;

    static Popover* New(Ctx* cx, Str id);
    Popover* Trigger(El* trigger);
    Popover* Content(El* content);
    El* IntoEl();
};
} // namespace gpui
