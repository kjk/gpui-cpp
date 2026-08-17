/* Unstyled collapsible — crates/base/src/collapsible.rs */

#pragma once

#include "gpui/Gpui.h"

namespace gpui {

struct Collapsible {
    El* root = nullptr;
    bool open = false;

    static Collapsible* New(Arena* a);
    Collapsible* Open(bool v);
    Collapsible* Child(El* e);
    Collapsible* Content(El* e);
    El* IntoEl();
};
} // namespace gpui
