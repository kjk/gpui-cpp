#include "ui/spinner.h"

namespace gpui {

namespace component {

Spinner* Spinner::New(Ctx* cx) {
    Arena* a = cx->a;
    Spinner* s = ArenaNew<Spinner>(a);
    s->a = a;
    s->cx = cx;
    return s;
}

Spinner* Spinner::WithSize(UiSize s) {
    size = s;
    return this;
}
Spinner* Spinner::Size(float v) {
    px = v;
    return this;
}

Spinner* Spinner::Icon(IconName n) {
    icon = n;
    return this;
}

Spinner* Spinner::Color(Rgba c) {
    color = c;
    hasColor = true;
    return this;
}

El* Spinner::IntoEl() {
    const Theme& th = cx->theme();
    float dim = px > 0 ? px : UiSizePx(size);
    El* ic = IconEl(a, icon, dim);
    if (hasColor) {
        ic->Fg(color);
    } else {
        ic->Fg(th.mutedFg);
    }
    return Div(a)->W(dim)->H(dim)->ItemsCenter()->JustifyCenter()->Child(ic);
}

} // namespace component
} // namespace gpui
