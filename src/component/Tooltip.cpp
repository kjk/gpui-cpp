#include "component/Tooltip.h"

namespace gpui {

namespace component {

Tooltip* Tooltip::New(Arena* a, Str text) {
    Tooltip* t = ArenaNew<Tooltip>(a);
    t->a = a;
    t->text = text;
    return t;
}

El* Tooltip::IntoEl() {
    const Theme& th = ThemeNow();
    return gpui::Tooltip::New(a, StrL("tooltip"))
        ->PadX(8)
        ->H(28)
        ->ItemsCenter()
        ->Radius(6)
        ->Bg(th.foreground)
        ->Child(TextEl(a, text)->Font(12)->Fg(th.background));
}

} // namespace component
} // namespace gpui
