/* Unstyled avatar — crates/base/src/avatar.rs */

#pragma once

#include "gpui/Gpui.h"

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
