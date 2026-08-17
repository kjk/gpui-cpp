/* Themed scroll — crates/ui/src/scroll */

#pragma once

#include "component/Common.h"

namespace component {

struct Scrollable {
    Arena* a = nullptr;
    El* child = nullptr;
    float scrollY = 0;
    float h = 200;

    static Scrollable* New(Arena* a);
    Scrollable* Child(El* e);
    Scrollable* ScrollY(float v);
    Scrollable* H(float v);
    El* IntoEl();
};

} // namespace component
