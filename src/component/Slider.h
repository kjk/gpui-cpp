/* Themed slider — crates/ui/src/slider.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

struct Slider {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    float value = 0; // 0..1
    Func1<float> onChange;

    static Slider* New(Ctx* cx);
    Slider* Value(float v);
    Slider* OnChange(Func1<float> fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
