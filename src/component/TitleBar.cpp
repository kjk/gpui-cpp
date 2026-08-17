#include "component/TitleBar.h"

namespace gpui {

namespace component {

TitleBar* TitleBar::New(Ctx* cx, Str title) {
    Arena* a = cx->a;
    TitleBar* t = ArenaNew<TitleBar>(a);
    t->a = a;
    t->cx = cx;
    t->title = title;
    return t;
}
TitleBar* TitleBar::Right(El* e) {
    right = e;
    return this;
}

El* TitleBar::IntoEl() {
    const Theme& th = ThemeNow();
    El* bar = Div(a)
                  ->FlexRow()
                  ->H(34)
                  ->PadX(12)
                  ->ItemsCenter()
                  ->JustifyBetween()
                  ->Bg(th.titleBar)
                  ->BorderB(1, th.titleBarBorder);
    bar->Child(TextEl(a, title)->Font(13)->Fg(th.foreground));
    if (right) {
        bar->Child(right);
    }
    return bar;
}

} // namespace component
} // namespace gpui
