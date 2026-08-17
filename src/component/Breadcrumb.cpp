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

Breadcrumb* Breadcrumb::New(Arena* a) {
    Breadcrumb* b = ArenaNew<Breadcrumb>(a);
    b->a = a;
    return b;
}
Breadcrumb* Breadcrumb::Item(Str s) {
    if (n < 8) {
        items[n++] = s;
    }
    return this;
}
Breadcrumb* Breadcrumb::OnClick(Func1<int> fn) {
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
            CrumbBind* b = ArenaNew<CrumbBind>(a);
            b->fn = onClick;
            b->index = i;
            BindClick(hit, items[i], MkFunc0(&FireCrumb, b));
            row->Child(hit);
        } else {
            row->Child(t);
        }
    }
    return row;
}

} // namespace component
} // namespace gpui
