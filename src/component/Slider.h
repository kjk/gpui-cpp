/* Themed slider — crates/ui/src/slider.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

struct Slider {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    // Each slider on a page needs its own click id, or they share a hover
    // and the hit test cannot tell them apart.
    Str id = {};
    // The value, its limits and its scale, owned by the view — Rust's
    // `Entity<SliderState>`. The widget reads the percentages it has already
    // worked out and the window writes to it on a press or a drag.
    SliderState* state = nullptr;
    // reverse(): the fill runs from the far end back toward the thumb, for
    // "remaining capacity" readings.
    bool reverse = false;
    bool disabled = false;
    // vertical(): the track runs bottom-to-top, and `width` is its length.
    Axis axis = Axis::Horizontal;
    float width = 224;
    Listener onChange;

    static Slider* New(Ctx* cx, Str id, SliderState* state);
    Slider* Reverse(bool v = true);
    Slider* Vertical(bool v = true);
    Slider* WithAxis(Axis v);
    Slider* Disabled(bool v = true);
    Slider* W(float px);
    // cx.subscribe(&state, ..): the view hears SliderEvent::Change while the
    // slider moves and Release when the button comes back up.
    Slider* OnChange(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
