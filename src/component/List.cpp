#include "component/List.h"

namespace gpui {

namespace component {

struct ListBind {
    Func1<int> fn;
    int index = 0;
};
static void FireList(ListBind* b) {
    b->fn.Call(b->index);
}

List* List::New(Ctx* cx) {
    Arena* a = cx->a;
    List* l = ArenaNew<List>(a);
    l->a = a;
    l->cx = cx;
    return l;
}
List* List::Item(Str s) {
    if (n < 32) {
        items[n++] = s;
    }
    return this;
}
List* List::Selected(int i) {
    selected = i;
    return this;
}
List* List::OnSelect(Listener fn) {
    onSelect = fn;
    return this;
}

El* List::IntoEl() {
    const Theme& th = ThemeNow();
    El* col = Div(a)->FlexCol()->Border(1, th.border);
    for (int i = 0; i < n; i++) {
        El* row = Div(a)->H(32)->PadX(10)->ItemsCenter()->HoverBg(th.muted);
        if (i == selected) {
            row->Bg(th.accent);
        }
        row->Child(TextEl(a, items[i])->Font(13)->Fg(th.foreground));
        if (onSelect.IsValid()) {
            BindClick(row, items[i], ListenerArg(onSelect, i));
        }
        col->Child(row);
    }
    return col;
}

} // namespace component
} // namespace gpui
