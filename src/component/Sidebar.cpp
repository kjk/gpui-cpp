#include "component/Sidebar.h"

namespace gpui {

namespace component {

struct SideBind {
    Func1<int> fn;
    int index = 0;
};
static void FireSide(SideBind* b) {
    b->fn.Call(b->index);
}

Sidebar* Sidebar::New(Arena* a) {
    Sidebar* s = ArenaNew<Sidebar>(a);
    s->a = a;
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
Sidebar* Sidebar::OnSelect(Func1<int> fn) {
    onSelect = fn;
    return this;
}

El* Sidebar::IntoEl() {
    const Theme& th = ThemeNow();
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
            SideBind* b = ArenaNew<SideBind>(a);
            b->fn = onSelect;
            b->index = i;
            BindClick(row, items[i], MkFunc0(&FireSide, b));
        }
        col->Child(row);
    }
    return col;
}

} // namespace component
} // namespace gpui
