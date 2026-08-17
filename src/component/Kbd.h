/* Themed kbd — crates/ui/src/kbd.rs */

#pragma once

#include "component/Common.h"

namespace component {

struct Kbd {
    Arena* a = nullptr;
    Str stroke = {};
    bool appearance = true;
    bool outline = false;

    static Kbd* New(Arena* a, Str stroke);
    Kbd* Appearance(bool v);
    Kbd* Outline();
    El* IntoEl();
};

} // namespace component
