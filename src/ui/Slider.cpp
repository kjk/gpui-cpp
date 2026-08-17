#include "ui/Slider.h"
#include "ui/Primitive.h"

namespace gpui {

El* Slider::New(Ctx* cx, int clickId) {
    Arena* a = cx->a;
    return UiRoot(a, StrL("example-slider"), clickId);
}
El* SliderTrack::New(Ctx* cx) {
    Arena* a = cx->a;
    return Div(a);
}
El* SliderIndicator::New(Ctx* cx) {
    Arena* a = cx->a;
    return Div(a);
}
El* SliderThumb::New(Ctx* cx) {
    Arena* a = cx->a;
    return Div(a);
}
} // namespace gpui
