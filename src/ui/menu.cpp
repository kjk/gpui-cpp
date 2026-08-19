#include "ui/menu.h"

namespace gpui {

namespace component {

Menu* Menu::New(Ctx* cx) {
    Arena* a = cx->a;
    Menu* m = ArenaNew<Menu>(a);
    m->a = a;
    m->cx = cx;
    return m;
}
Menu* Menu::Item(Str s) {
    if (n < 8) {
        items[n++] = s;
    }
    return this;
}
Menu* Menu::OnClick(Listener fn) {
    onClick = fn;
    return this;
}

El* Menu::IntoEl() {
    const Theme& th = cx->theme();
    El* col = Div(a)
                  ->FlexCol()
                  ->W(180)
                  ->Border(1, th.border)
                  ->Bg(th.background)
                  ->Radius(th.radius);
    for (int i = 0; i < n; i++) {
        El* row =
            Div(a)->H(28)->PadX(10)->ItemsCenter()->HoverBg(th.muted)->Child(
                TextEl(a, items[i])->Font(13)->Fg(th.foreground));
        if (onClick.IsValid()) {
            BindClick(row, items[i], ListenerArg(onClick, i));
        }
        col->Child(row);
    }
    return col;
}

} // namespace component
} // namespace gpui
