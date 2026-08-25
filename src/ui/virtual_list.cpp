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
VirtualList* VirtualList::Id(Str v) {
    id = v;
    return this;
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
VirtualList* VirtualList::ScrollX(float v) {
    scrollX = v;
    return this;
}
VirtualList* VirtualList::Sizes(const float* v) {
    sizes = v;
    return this;
}
VirtualList* VirtualList::Handle(VirtualListScrollHandle* h) {
    handle = h;
    return this;
}
VirtualList* VirtualList::Scroll(int sid, Listener l) {
    scrollId = sid;
    onScroll = l;
    return this;
}
VirtualList* VirtualList::Axis(ScrollAxis v) {
    axis = v;
    return this;
}
VirtualList* VirtualList::Pad(float v) {
    pad = v;
    return this;
}
VirtualList* VirtualList::Row(El* (*fn)(Ctx*, int)) {
    row = fn;
    return this;
}

El* VirtualList::IntoEl() {
    const Theme& th = cx->theme();
    // The layout is where the handle is answered: it learns how many items
    // there are and how much of them is showing, a pending scroll_to_item is
    // applied against that, and the offset is clamped to the list.
    float offset = scrollY;
    if (handle) {
        VirtualListHandleLayout(handle, sizes, count, rowH, viewH);
        offset = handle->offset;
    }
    // The rows the viewport can show, and a spacer at each end standing in
    // for the ones that were not built — without the second one the list
    // would scroll only as far as the last row it made.
    VirtualRange range =
        sizes ? VirtualListVisibleRange(sizes, count, offset, viewH)
              : VirtualListVisibleRows(count, rowH, offset, viewH);
    int first = range.first;
    El* list = Div(a)->FlexCol();
    if (first > 0) {
        float before = sizes ? VirtualListItemOrigin(sizes, count, first)
                             : (float)first * rowH;
        list->Child(Div(a)->H(before));
    }
    for (int ix = first; ix < range.end; ix++) {
        if (row) {
            list->Child(row(cx, ix));
        } else {
            float h = sizes ? sizes[ix] : rowH;
            list->Child(Div(a)->H(h)->PadX(8)->ItemsCenter()->Child(
                TextEl(a, StrDup(a, fmt("Item %d", ix)))
                    ->Font(12)
                    ->Fg(th.foreground)));
        }
    }
    if (range.end < count) {
        float content =
            sizes ? VirtualListContentSize(sizes, count) : (float)count * rowH;
        float built = sizes ? VirtualListItemOrigin(sizes, count, range.end)
                            : (float)range.end * rowH;
        list->Child(Div(a)->H(content - built));
    }
    El* e = gpui::VirtualList::New(cx, id)
                ->H(viewH + pad * 2)
                ->ClipY()
                ->ScrollY(offset);
    if (pad > 0) {
        e->Pad(pad);
    }
    // Both axes: a row wider than the viewport slides under it rather than
    // being cut, which is what the story's Axis: Both asks for.
    e->ClipX()->ScrollX(scrollX);
    // The axis names the bars, not the scrolling: a list set to Vertical
    // still slides sideways under the wheel, it just does not draw the bar
    // along the bottom. That is what Rust's `.scrollbar(&handle, axis)` does,
    // since the bar layer is a sibling of the list rather than part of it.
    if (axis == ScrollAxis::Vertical) {
        e->HideScrollbarX();
    } else if (axis == ScrollAxis::Horizontal) {
        e->HideScrollbarY();
    }
    if (scrollId) {
        e->ScrollId(scrollId)->OnScroll(onScroll);
    }
    return e->Child(list);
}

} // namespace component
} // namespace gpui
