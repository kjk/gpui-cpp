#include "component/Menu.h"

namespace component {

struct MenuBind {
    Func1<int> fn;
    int index = 0;
};
static void FireMenu(MenuBind* b) {
    b->fn.Call(b->index);
}

Menu* Menu::New(Arena* a) {
    Menu* m = ::New<Menu>(a);
    m->a = a;
    return m;
}
Menu* Menu::Item(Str s) {
    if (n < 8) {
        items[n++] = s;
    }
    return this;
}
Menu* Menu::OnClick(Func1<int> fn) {
    onClick = fn;
    return this;
}

El* Menu::IntoEl() {
    const Theme& th = ThemeNow();
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
            MenuBind* b = ::New<MenuBind>(a);
            b->fn = onClick;
            b->index = i;
            BindClick(row, items[i], MkFunc0(&FireMenu, b));
        }
        col->Child(row);
    }
    return col;
}

} // namespace component
