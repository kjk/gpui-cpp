#include "component/Skeleton.h"

namespace gpui {

namespace component {

Skeleton* Skeleton::New(Ctx* cx) {
    Arena* a = cx->a;
    Skeleton* s = ArenaNew<Skeleton>(a);
    s->a = a;
    s->cx = cx;
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
    const Theme& th = cx->theme();
    Rgba bg = th.skeleton;
    if (secondary) {
        bg = RgbaOpacity(bg, 0.5f);
    }
    return Div(a)->W(w)->H(h)->Bg(bg)->Radius(4);
}

} // namespace component
} // namespace gpui
