/* Themed text / markdown view — crates/ui/src/text */

#pragma once

#include "component/Common.h"

namespace component {

struct TextView {
    Arena* a = nullptr;
    Str source = {};

    static TextView* New(Arena* a, Str source);
    El* IntoEl();
};

} // namespace component
