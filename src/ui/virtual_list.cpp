#include "ui/virtual_list.h"

namespace gpui {

namespace component {

VirtualList* VirtualList::New(Ctx* cx, int count) {
    Arena* a = cx->a;
    VirtualList* v = ArenaNew<VirtualList>(a);
    v->a = a;
    v->cx = cx;
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
    const Theme& th = cx->theme();
    // The rows the viewport can show, and a spacer at each end standing in
    // for the ones that were not built — without the second one the list
    // would scroll only as far as the last row it made.
    VirtualRange range = VirtualListVisibleRows(count, rowH, scrollY, viewH);
    int first = range.first;
    El* list = Div(a)->FlexCol();
    if (first > 0) {
        list->Child(Div(a)->H((float)first * rowH));
    }
    for (int ix = first; ix < range.end; ix++) {
        if (row) {
            list->Child(row(a, ix));
        } else {
            list->Child(Div(a)->H(rowH)->PadX(8)->ItemsCenter()->Child(
                TextEl(a, StrDup(a, fmt("Item %d", ix)))
                    ->Font(12)
                    ->Fg(th.foreground)));
        }
    }
    if (range.end < count) {
        list->Child(Div(a)->H((float)(count - range.end) * rowH));
    }
    return gpui::VirtualList::New(cx, StrL("vlist"))
        ->H(viewH)
        ->ClipY()
        ->ScrollY(scrollY)
        ->Child(list);
}

} // namespace component
} // namespace gpui
