#include "component/Progress.h"

namespace component {

Progress* Progress::New(Arena* a) {
    Progress* p = ::New<Progress>(a);
    p->a = a;
    return p;
}

Progress* Progress::Value(float v) {
    value = v;
    if (value < 0) {
        value = 0;
    }
    if (value > 100) {
        value = 100;
    }
    return this;
}
Progress* Progress::W(float v) {
    w = v;
    return this;
}
Progress* Progress::H(float v) {
    h = v;
    return this;
}

El* Progress::IntoEl() {
    const Theme& th = ThemeNow();
    return ::Progress::New(a, StrL("progress"))
        ->W(w)
        ->Child(::ProgressTrack::New(a)->W(w)->H(h)->Radius(h * 0.5f)->Bg(RgbaOpacity(th.progress, 0.2f))->Child(
            ::ProgressIndicator::New(a)->W(w * (value / 100.f))->H(h)->Radius(h * 0.5f)->Bg(th.progress)));
}

ProgressCircle* ProgressCircle::New(Arena* a) {
    ProgressCircle* p = ::New<ProgressCircle>(a);
    p->a = a;
    return p;
}
ProgressCircle* ProgressCircle::Value(float v) {
    value = v;
    return this;
}
ProgressCircle* ProgressCircle::Size(float v) {
    size = v;
    return this;
}

El* ProgressCircle::IntoEl() {
    return Div(a)
        ->W(size)
        ->H(size)
        ->ItemsCenter()
        ->JustifyCenter()
        ->Child(TextEl(a, str::Dup(a, fmt("%.0f%%", value)))->Font(12)->Fg(ThemeNow().foreground));
}

} // namespace component
