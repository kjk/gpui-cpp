/* Unstyled slider — crates/base/src/slider.rs */

#pragma once

#include "gpui/Gpui.h"

struct Slider {
    static El* New(Arena* a, int clickId = 0);
};
struct SliderTrack {
    static El* New(Arena* a);
};
struct SliderIndicator {
    static El* New(Arena* a);
};
struct SliderThumb {
    static El* New(Arena* a);
};
