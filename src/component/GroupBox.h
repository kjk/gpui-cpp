/* Themed group box — crates/ui/src/group_box.rs */

#pragma once

#include "component/Common.h"

namespace component {

struct GroupBox {
    Arena* a = nullptr;
    Str title = {};
    El* child = nullptr;

    static GroupBox* New(Arena* a, Str title);
    GroupBox* Child(El* e);
    El* IntoEl();
};

} // namespace component
