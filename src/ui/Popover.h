/* Unstyled popover — crates/base/src/popover.rs */

#pragma once

#include "gpui/Gpui.h"

struct Popover {
    El* root = nullptr;

    static Popover* New(Arena* a, Str id);
    Popover* Trigger(El* trigger);
    Popover* Content(El* content);
    El* IntoEl();
};
