/* Themed tooltip — crates/ui/src/tooltip.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

struct Tooltip {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str text = {};

    static Tooltip* New(Ctx* cx, Str text);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
