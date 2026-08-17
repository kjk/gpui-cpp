/* Themed skeleton — crates/ui/src/skeleton.rs */

#pragma once

#include "component/Common.h"

namespace gpui {

namespace component {

struct Skeleton {
    Arena* a = nullptr;
    bool secondary = false;
    float w = kFill;
    float h = 16;

    static Skeleton* New(Arena* a);
    Skeleton* Secondary();
    Skeleton* W(float v);
    Skeleton* H(float v);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
