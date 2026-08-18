#include "component/Sidebar.h"

namespace gpui {

namespace component {

Sidebar* Sidebar::New(Ctx* cx) {
    Arena* a = cx->a;
    Sidebar* s = ArenaNew<Sidebar>(a);
    s->a = a;
    s->cx = cx;
    return s;
}
Sidebar* Sidebar::Title(Str s) {
    title = s;
    return this;
}
Sidebar* Sidebar::Item(Str s) {
    if (n < 8) {
        items[n++] = s;
    }
    return this;
}
Sidebar* Sidebar::Selected(int i) {
    selected = i;
    return this;
}
Sidebar* Sidebar::Collapsed(bool v) {
    collapsed = v;
    return this;
}
Sidebar* Sidebar::OnSelect(Listener fn) {
    onSelect = fn;
    return this;
}

El* Sidebar::IntoEl() {
    const Theme& th = cx->theme();
    El* col = Div(a)
                  ->FlexCol()
                  ->W(collapsed ? 48.f : 220.f)
                  ->H(kFill)
                  ->Pad(8)
                  ->Gap(4)
                  ->Bg(th.sidebar);
    if (title.s && !collapsed) {
        col->Child(TextEl(a, title)->Font(14)->Semibold()->Fg(th.sidebarFg));
    }
    for (int i = 0; i < n; i++) {
        El* row = Div(a)->H(32)->PadX(8)->ItemsCenter()->Radius(6)->HoverBg(
            th.secondaryHover);
        if (i == selected) {
            row->Bg(th.accent);
        }
        row->Child(TextEl(a, collapsed ? StrL("•") : items[i])
                       ->Font(13)
                       ->Fg(th.sidebarFg));
        if (onSelect.IsValid()) {
            BindClick(row, items[i], ListenerArg(onSelect, i));
        }
        col->Child(row);
    }
    return col;
}

} // namespace component
} // namespace gpui
