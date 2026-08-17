/* Unstyled toast — crates/base/src/toast.rs */

#include "gpui/Gpui.h"

namespace gpui {

struct Toast {
    static El* New(Ctx* cx, Str id);
};
} // namespace gpui
