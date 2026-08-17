/* Unstyled avatar — crates/base/src/avatar.rs */

#include "gpui/Gpui.h"

namespace gpui {

struct Avatar {
    El* root = nullptr;
    El* fallback = nullptr;

    static Avatar* New(Arena* a);
    Avatar* Size(float px);
    Avatar* Fallback(El* fallback);
    El* IntoEl();
};

struct AvatarFallback {
    static El* New(Arena* a);
};
} // namespace gpui
