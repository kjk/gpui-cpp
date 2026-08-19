#include "ui/kbd.h"

namespace gpui {

namespace component {

Kbd* Kbd::New(Ctx* cx, Str stroke) {
    Arena* a = cx->a;
    Kbd* k = ArenaNew<Kbd>(a);
    k->a = a;
    k->cx = cx;
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
    const Theme& th = cx->theme();
    if (!appearance) {
        return TextEl(a, stroke)->Font(12)->Fg(th.mutedFg);
    }
    // The plain chip is a muted wash with no border; outline swaps to the
    // window background inside one. px_1 / py_0p5 / min_w_5 / radius half.
    El* e = Div(a)
                ->PadX(4)
                ->PadY(2)
                ->MinW(20)
                ->ItemsCenter()
                ->JustifyCenter()
                ->Radius(th.radius * 0.5f)
                ->Bg(th.muted);
    if (outline) {
        e->Bg(th.background)->Border(1, th.border);
    }
    e->Child(TextEl(a, stroke)->Font(12)->LineHeight(1.f)->Fg(th.mutedFg));
    return e;
}

} // namespace component
} // namespace gpui
