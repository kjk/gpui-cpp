/* Themed avatar — crates/ui/src/avatar */

#pragma once

#include "component/Common.h"

namespace component {

struct Avatar {
    Arena* a = nullptr;
    Str initials = {};
    Rgba bg = {};
    float size = 34;

    static Avatar* New(Arena* a);
    Avatar* Initials(Str s);
    Avatar* Bg(Rgba c);
    Avatar* Size(float v);
    El* IntoEl();
};

} // namespace component
