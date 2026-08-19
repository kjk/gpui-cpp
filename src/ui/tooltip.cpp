#include "ui/tooltip.h"

namespace gpui {

namespace component {

Tooltip* Tooltip::New(Ctx* cx, Str text) {
    Arena* a = cx->a;
    Tooltip* t = ArenaNew<Tooltip>(a);
    t->a = a;
    t->cx = cx;
    t->text = text;
    return t;
}

El* Tooltip::IntoEl() {
    const Theme& th = cx->theme();
    return gpui::Tooltip::New(cx, StrL("tooltip"))
        ->PadX(8)
        ->H(28)
        ->ItemsCenter()
        ->Radius(6)
        ->Bg(th.foreground)
        ->Child(TextEl(a, text)->Font(12)->Fg(th.background));
}

} // namespace component
} // namespace gpui
