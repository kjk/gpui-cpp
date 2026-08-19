/* Unstyled slider — crates/base/src/slider.rs */

#include "gpui/Gpui.h"

namespace gpui {

struct Slider {
    static El* New(Ctx* cx, int clickId = 0);
};
// The box that catches a press. Binding the state here is what Rust's
// `SliderTrack::on_mouse_down` + `on_drag_move` do.
struct SliderTrack {
    static El* New(Ctx* cx, SliderState* state = nullptr,
                   Axis axis = Axis::Horizontal);
};
struct SliderIndicator {
    static El* New(Ctx* cx);
};
struct SliderThumb {
    static El* New(Ctx* cx);
};
} // namespace gpui
