#include "component/Kbd.h"

namespace component {

Kbd* Kbd::New(Arena* a, Str stroke) {
    Kbd* k = ::New<Kbd>(a);
    k->a = a;
    k->stroke = stroke;
    return k;
}

Kbd* Kbd::Appearance(bool v) {
    appearance = v;
    return this;
}

Kbd* Kbd::Outline() {
    outline = true;
    return this;
}

El* Kbd::IntoEl() {
    const Theme& th = ThemeNow();
    if (!appearance) {
        return TextEl(a, stroke)->Font(12)->Fg(th.mutedFg);
    }
    El* e = Div(a)->PadX(6)->PadY(2)->ItemsCenter()->JustifyCenter()->Radius(4);
    if (outline) {
        e->Border(1, th.border);
    } else {
        e->Bg(th.muted)->Border(1, th.border);
    }
    e->Child(TextEl(a, stroke)->Font(11)->Fg(th.foreground));
    return e;
}

} // namespace component
