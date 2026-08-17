#include "ui/Slider.h"
#include "ui/Primitive.h"

namespace gpui {

El* Slider::New(Arena* a, int clickId) {
    return UiRoot(a, StrL("example-slider"), clickId);
}
El* SliderTrack::New(Arena* a) {
    return Div(a);
}
El* SliderIndicator::New(Arena* a) {
    return Div(a);
}
El* SliderThumb::New(Arena* a) {
    return Div(a);
}
} // namespace gpui
