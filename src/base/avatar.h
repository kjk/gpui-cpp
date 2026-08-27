#ifndef GPUI_BASE_AVATAR_H_
#define GPUI_BASE_AVATAR_H_
/* Unstyled avatar — crates/base/src/avatar.rs */

#include "gpui/gpui.h"

namespace gpui {

// Rust renders `image.or_else(fallback)`: the fallback is the *alternative* to
// a picture, not something drawn beside or under one. An avatar with both set
// shows the image alone, which is what makes the fallback a fallback.
struct Avatar {
    El* root = nullptr;
    El* image = nullptr;
    El* fallback = nullptr;

    static Avatar* New(Ctx* cx);
    Avatar* Size(float px);
    Avatar* Image(El* image);
    Avatar* Fallback(El* fallback);
    El* IntoEl();
};

struct AvatarImage {
    static El* New(Ctx* cx);
};
struct AvatarFallback {
    static El* New(Ctx* cx);
};
} // namespace gpui
#endif // GPUI_BASE_AVATAR_H_
