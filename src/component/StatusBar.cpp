#include "component/StatusBar.h"

namespace gpui {

namespace component {

StatusBar* StatusBar::New(Ctx* cx) {
    Arena* a = cx->a;
    StatusBar* s = ArenaNew<StatusBar>(a);
    s->a = a;
    s->cx = cx;
    return s;
}
StatusBar* StatusBar::Left(Str s) {
    left = s;
    return this;
}
StatusBar* StatusBar::Right(Str s) {
    right = s;
    return this;
}

El* StatusBar::IntoEl() {
    const Theme& th = cx->theme();
    return Div(a)
        ->FlexRow()
        ->H(28)
        ->PadX(12)
        ->ItemsCenter()
        ->JustifyBetween()
        ->Bg(th.titleBar)
        ->BorderT(1, th.border)
        ->Child(TextEl(a, left)->Font(12)->Fg(th.mutedFg))
        ->Child(TextEl(a, right)->Font(12)->Fg(th.mutedFg));
}

} // namespace component
} // namespace gpui
