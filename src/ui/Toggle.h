/* Unstyled toggle / toggle group — crates/base/src/toggle.rs */

#pragma once

#include "gpui/Gpui.h"

struct Toggle {
    static El* New(Arena* a, Str id, int clickId = 0);
};

struct ToggleGroup {
    static El* New(Arena* a, Str id);
};
