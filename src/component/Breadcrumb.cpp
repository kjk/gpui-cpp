#include "component/Breadcrumb.h"

namespace gpui {

namespace component {

struct CrumbBind {
    Func1<int> fn;
    int index = 0;
};
static void FireCrumb(CrumbBind* b) {
    b->fn.Call(b->index);
}

Breadcrumb* Breadcrumb::New(Ctx* cx) {
    Arena* a = cx->a;
    Breadcrumb* b = ArenaNew<Breadcrumb>(a);
    b->a = a;
    b->cx = cx;
    return b;
}
Breadcrumb* Breadcrumb::Item(Str s) {
    if (n < 8) {
        items[n++] = s;
    }
    return this;
}
Breadcrumb* Breadcrumb::OnClick(Listener fn) {
    onClick = fn;
    return this;
}

El* Breadcrumb::IntoEl() {
    const Theme& th = ThemeNow();
    El* row = Div(a)->FlexRow()->ItemsCenter()->Gap(6);
    for (int i = 0; i < n; i++) {
        if (i) {
            row->Child(IconEl(a, IconName::ChevronRight, 12)->Fg(th.mutedFg));
        }
        bool last = i == n - 1;
        El* t = TextEl(a, items[i])
                    ->Font(13)
                    ->Fg(last ? th.foreground : th.mutedFg);
        if (onClick.IsValid()) {
            El* hit = Div(a)->Child(t);
            BindClick(hit, items[i], ListenerArg(onClick, i));
            row->Child(hit);
        } else {
            row->Child(t);
        }
    }
    return row;
}

} // namespace component
} // namespace gpui
