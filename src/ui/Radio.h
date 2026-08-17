/* Unstyled radio / radio group — crates/base/src/radio.rs */

#pragma once

#include "gpui/Gpui.h"

struct Radio {
    static El* New(Arena* a, Str id, int clickId = 0);
};

struct RadioGroup {
    static El* New(Arena* a, Str id);
};
