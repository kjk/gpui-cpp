#include "component/Slider.h"

namespace gpui {

namespace component {

Slider* Slider::New(Ctx* cx) {
    Arena* a = cx->a;
    Slider* s = ArenaNew<Slider>(a);
    s->a = a;
    s->cx = cx;
    return s;
}
Slider* Slider::Value(float v) {
    value = v;
    return this;
}
Slider* Slider::OnChange(Listener fn) {
    onChange = fn;
    return this;
}

El* Slider::IntoEl() {
    const Theme& th = ThemeNow();
    float p = value;
    if (p < 0) {
        p = 0;
    }
    if (p > 1) {
        p = 1;
    }
    float w = 224;
    El* track = SliderTrack::New(cx)->W(w)->H(28);
    track->Child(
        Div(a)->Absolute()->Top(13)->Left(0)->W(w)->H(2)->Bg(th.secondary));
    track->Child(SliderIndicator::New(cx)
                     ->Absolute()
                     ->Top(13)
                     ->Left(0)
                     ->W(w * p)
                     ->H(2)
                     ->Bg(th.primary));
    track->Child(SliderThumb::New(cx)
                     ->Absolute()
                     ->Top(7)
                     ->Left(w * p - 7)
                     ->W(14)
                     ->H(14)
                     ->Radius(7)
                     ->Bg(th.background)
                     ->Border(1, th.primary));
    return gpui::Slider::New(cx, HashClickId(StrL("slider")))
        ->W(w)
        ->H(28)
        ->Child(track);
}

} // namespace component
} // namespace gpui
