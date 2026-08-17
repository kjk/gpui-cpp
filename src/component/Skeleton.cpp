#include "component/Skeleton.h"

namespace component {

Skeleton* Skeleton::New(Arena* a) {
    Skeleton* s = ::New<Skeleton>(a);
    s->a = a;
    return s;
}

Skeleton* Skeleton::Secondary() {
    secondary = true;
    return this;
}

Skeleton* Skeleton::W(float v) {
    w = v;
    return this;
}

Skeleton* Skeleton::H(float v) {
    h = v;
    return this;
}

El* Skeleton::IntoEl() {
    const Theme& th = ThemeNow();
    Rgba bg = th.skeleton;
    if (secondary) {
        bg = RgbaOpacity(bg, 0.5f);
    }
    return Div(a)->W(w)->H(h)->Bg(bg)->Radius(4);
}

} // namespace component
