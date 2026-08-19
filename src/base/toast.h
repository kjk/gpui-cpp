/* Unstyled toast — crates/base/src/toast.rs */

#include "gpui/gpui.h"

namespace gpui {

struct Toast {
    static El* New(Ctx* cx, Str id);
};
} // namespace gpui
