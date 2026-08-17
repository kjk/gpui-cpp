/* Unstyled popover — crates/base/src/popover.rs */

#include "gpui/Gpui.h"

namespace gpui {

struct Popover {
    El* root = nullptr;

    static Popover* New(Arena* a, Str id);
    Popover* Trigger(El* trigger);
    Popover* Content(El* content);
    El* IntoEl();
};
} // namespace gpui
