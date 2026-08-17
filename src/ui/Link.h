/* Unstyled link — crates/base/src/link.rs
   href is target data. Navigation is application-owned (showcase logs the
   path). */

#include "gpui/Gpui.h"

namespace gpui {

struct Link {
    static El* New(Ctx* cx, Str id, int clickId = 0);
};
} // namespace gpui
