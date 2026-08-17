#include "component/Spinner.h"

namespace component {

Spinner* Spinner::New(Arena* a) {
    Spinner* s = ::New<Spinner>(a);
    s->a = a;
    return s;
}

Spinner* Spinner::WithSize(UiSize s) {
    size = s;
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
    const Theme& th = ThemeNow();
    float px = UiSizePx(size);
    El* ic = IconEl(a, icon, px);
    if (hasColor) {
        ic->Fg(color);
    } else {
        ic->Fg(th.mutedFg);
    }
    return Div(a)->W(px)->H(px)->ItemsCenter()->JustifyCenter()->Child(ic);
}

} // namespace component
