/* Themed slider — crates/ui/src/slider.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

struct Slider {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    // 0..1. A range slider fills between lo and hi; a single one from the
    // start of the track to `value`.
    float value = 0;
    float lo = 0;
    bool range = false;
    // reverse(): the fill runs from the far end back toward the thumb, for
    // "remaining capacity" readings.
    bool reverse = false;
    bool disabled = false;
    float width = 224;
    Listener onChange;

    static Slider* New(Ctx* cx);
    Slider* Value(float v);
    Slider* Range(float low, float high);
    Slider* Reverse(bool v = true);
    Slider* Disabled(bool v = true);
    Slider* W(float px);
    Slider* OnChange(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
