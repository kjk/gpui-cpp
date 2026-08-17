/* Unstyled slider — crates/base/src/slider.rs */

#include "gpui/Gpui.h"

namespace gpui {

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
} // namespace gpui
