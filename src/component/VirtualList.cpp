#include "component/VirtualList.h"

namespace gpui {

namespace component {

VirtualList* VirtualList::New(Arena* a, int count) {
    VirtualList* v = ArenaNew<VirtualList>(a);
    v->a = a;
    v->count = count;
    return v;
}
VirtualList* VirtualList::RowH(float v) {
    rowH = v;
    return this;
}
VirtualList* VirtualList::ViewH(float v) {
    viewH = v;
    return this;
}
VirtualList* VirtualList::ScrollY(float v) {
    scrollY = v;
    return this;
}
VirtualList* VirtualList::Row(El* (*fn)(Arena*, int)) {
    row = fn;
    return this;
}

El* VirtualList::IntoEl() {
    const Theme& th = ThemeNow();
    int first = (int)(scrollY / rowH);
    if (first < 0) {
        first = 0;
    }
    int visible = (int)(viewH / rowH) + 2;
    El* list = Div(a)->FlexCol();
    if (first > 0) {
        list->Child(Div(a)->H((float)first * rowH));
    }
    for (int i = 0; i < visible; i++) {
        int ix = first + i;
        if (ix >= count) {
            break;
        }
        if (row) {
            list->Child(row(a, ix));
        } else {
            list->Child(Div(a)->H(rowH)->PadX(8)->ItemsCenter()->Child(
                TextEl(a, StrDup(a, fmt("Item %d", ix)))
                    ->Font(12)
                    ->Fg(th.foreground)));
        }
    }
    return gpui::VirtualList::New(a, StrL("vlist"))
        ->H(viewH)
        ->ClipY()
        ->Child(list);
}

} // namespace component
} // namespace gpui
