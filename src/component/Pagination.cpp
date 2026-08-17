#include "component/Pagination.h"
#include "component/Button.h"

namespace component {

struct PageBind {
    Func1<int> fn;
    int page = 1;
};
static void FirePage(PageBind* b) {
    b->fn.Call(b->page);
}

Pagination* Pagination::New(Arena* a, int page, int total) {
    Pagination* p = ::New<Pagination>(a);
    p->a = a;
    p->page = page;
    p->total = total;
    return p;
}
Pagination* Pagination::OnChange(Func1<int> fn) {
    onChange = fn;
    return this;
}

El* Pagination::IntoEl() {
    El* row = ::Pagination::New(a, StrL("pagination"))
                  ->FlexRow()
                  ->Gap(8)
                  ->ItemsCenter();
    int n = total > 12 ? 12 : total;
    if (n < 1) {
        n = 1;
    }
    for (int i = 1; i <= n; i++) {
        Button* b = Button::New(a, str::Dup(a, fmt("page-%d", i)))
                        ->Label(str::Dup(a, fmt("%d", i)))
                        ->Compact();
        if (i == page) {
            b->Primary();
        }
        if (onChange.IsValid()) {
            PageBind* bind = ::New<PageBind>(a);
            bind->fn = onChange;
            bind->page = i;
            b->OnClick(MkFunc0(&FirePage, bind));
        }
        row->Child(b->IntoEl());
    }
    return row;
}

} // namespace component
