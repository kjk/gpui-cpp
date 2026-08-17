/* Unstyled slider — crates/base/src/slider.rs */

#include "gpui/Gpui.h"

namespace gpui {

struct Slider {
    static El* New(Ctx* cx, int clickId = 0);
};
struct SliderTrack {
    static El* New(Ctx* cx);
};
struct SliderIndicator {
    static El* New(Ctx* cx);
};
struct SliderThumb {
    static El* New(Ctx* cx);
};
} // namespace gpui
