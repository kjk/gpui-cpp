/* Themed popover — crates/ui/src/popover.rs */

#pragma once

#include "component/Common.h"

namespace component {

struct Popover {
    Arena* a = nullptr;
    El* trigger = nullptr;
    El* content = nullptr;
    bool open = false;

    static Popover* New(Arena* a);
    Popover* Trigger(El* e);
    Popover* Content(El* e);
    Popover* Open(bool v);
    El* IntoEl();
};

} // namespace component
