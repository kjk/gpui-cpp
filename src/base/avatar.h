/* Unstyled avatar — crates/base/src/avatar.rs */

#include "gpui/gpui.h"

namespace gpui {

struct Avatar {
    El* root = nullptr;
    El* fallback = nullptr;

    static Avatar* New(Ctx* cx);
    Avatar* Size(float px);
    Avatar* Fallback(El* fallback);
    El* IntoEl();
};

struct AvatarFallback {
    static El* New(Ctx* cx);
};
} // namespace gpui
