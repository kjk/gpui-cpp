#include "base/virtual_list.h"

namespace gpui {

VirtualRange VirtualListVisibleRange(const float* sizes, int count,
                                     float offset, float viewport) {
    VirtualRange r;
    if (count <= 0) {
        return r;
    }
    // The first item whose end has passed the top edge.
    float cumulative = 0;
    for (int i = 0; i < count; i++) {
        cumulative += sizes[i];
        if (cumulative > offset) {
            r.first = i;
            break;
        }
    }
    // The first whose end has passed the bottom edge. Rust counts this one in
    // and then adds one more, so the row being scrolled into is already built
    // by the time it arrives.
    cumulative = 0;
    int last = 0;
    for (int i = 0; i < count; i++) {
        cumulative += sizes[i];
        if (cumulative > offset + viewport) {
            last = i + 1;
            break;
        }
    }
    if (last == 0) {
        // Nothing crossed the bottom edge, so the rest of the list fits.
        last = count;
    } else {
        last += 1;
    }
    r.end = last < count ? last : count;
    return r;
}

VirtualRange VirtualListVisibleRows(int count, float rowSize, float offset,
                                    float viewport) {
    VirtualRange r;
    if (count <= 0 || rowSize <= 0) {
        return r;
    }
    // The first row whose end has passed the top edge. Scrolled past the end
    // of the list there is no such row, and Rust's scan leaves the first at
    // zero rather than clamping.
    int first = (int)(offset / rowSize);
    if (first < 0 || first >= count) {
        first = 0;
    }
    r.first = first;
    // The first whose end has passed the bottom edge, counted in, plus the
    // spare Rust adds after it.
    int last = (int)((offset + viewport) / rowSize);
    if (last < 0) {
        last = 0;
    }
    int end = last >= count ? count : last + 2;
    r.end = end < count ? end : count;
    if (r.end < r.first) {
        r.end = r.first;
    }
    return r;
}

float VirtualListItemOrigin(const float* sizes, int count, int ix) {
    float at = 0;
    int n = ix < count ? ix : count;
    for (int i = 0; i < n; i++) {
        at += sizes[i];
    }
    return at;
}

float VirtualListContentSize(const float* sizes, int count) {
    return VirtualListItemOrigin(sizes, count, count);
}

float VirtualListScrollTo(float origin, float size, float offset,
                          float viewport, float contentSize,
                          ScrollStrategy strategy) {
    float want = offset;
    if (strategy == ScrollStrategy::Center) {
        want = origin + size * 0.5f - viewport * 0.5f;
    } else if (origin < offset) {
        want = origin;
    } else if (origin + size > offset + viewport) {
        want = origin + size - viewport;
    }
    float most = contentSize - viewport;
    if (want > most) {
        want = most;
    }
    return want < 0 ? 0 : want;
}

float VirtualListScrollToItem(const float* sizes, int count, int ix,
                              float offset, float viewport,
                              ScrollStrategy strategy) {
    if (ix < 0 || ix >= count) {
        return offset;
    }
    return VirtualListScrollTo(VirtualListItemOrigin(sizes, count, ix),
                               sizes[ix], offset, viewport,
                               VirtualListContentSize(sizes, count), strategy);
}

float VirtualListScrollToRow(int count, float rowSize, int ix, float offset,
                             float viewport, ScrollStrategy strategy) {
    if (ix < 0 || ix >= count) {
        return offset;
    }
    return VirtualListScrollTo((float)ix * rowSize, rowSize, offset, viewport,
                               (float)count * rowSize, strategy);
}

void VirtualListScrollToItemDeferred(VirtualListScrollHandle* h, int ix,
                                     ScrollStrategy strategy) {
    VirtualListScrollToItemDeferredWithOffset(h, ix, strategy, 0);
}

void VirtualListScrollToItemDeferredWithOffset(VirtualListScrollHandle* h,
                                               int ix, ScrollStrategy strategy,
                                               int offset) {
    if (!h) {
        return;
    }
    h->pending = true;
    h->pendingIx = ix;
    h->pendingStrategy = strategy;
    h->pendingOffset = offset;
}

void VirtualListScrollToBottomDeferred(VirtualListScrollHandle* h) {
    if (!h) {
        return;
    }
    // saturating_sub: an empty list scrolls to item 0, which is nowhere.
    int last = h->itemsCount > 0 ? h->itemsCount - 1 : 0;
    VirtualListScrollToItemDeferred(h, last, ScrollStrategy::Top);
}

bool VirtualListHandleLayout(VirtualListScrollHandle* h, const float* sizes,
                             int count, float itemSize, float viewport) {
    if (!h) {
        return false;
    }
    float before = h->offset;
    h->itemsCount = count;
    h->viewport = viewport;
    h->contentSize =
        sizes ? VirtualListContentSize(sizes, count) : (float)count * itemSize;
    if (h->pending) {
        // The request is taken, whether or not there is an item to answer it
        // with: Rust's `take()` clears it either way.
        h->pending = false;
        int ix = h->pendingIx + h->pendingOffset;
        if (ix >= 0 && ix < count) {
            h->offset =
                sizes ? VirtualListScrollToItem(sizes, count, ix, h->offset,
                                                viewport, h->pendingStrategy)
                      : VirtualListScrollToRow(count, itemSize, ix, h->offset,
                                               viewport, h->pendingStrategy);
        }
    }
    // The clamp: there is nothing to see past either end, however the offset
    // got there — a list that shrank under a scrolled view comes back.
    float most = h->contentSize - viewport;
    if (h->offset > most) {
        h->offset = most;
    }
    if (h->offset < 0) {
        h->offset = 0;
    }
    return h->offset != before;
}

VirtualRange VirtualListHandleRange(const VirtualListScrollHandle* h,
                                    const float* sizes, int count,
                                    float itemSize) {
    if (!h) {
        return {};
    }
    if (sizes) {
        return VirtualListVisibleRange(sizes, count, h->offset, h->viewport);
    }
    return VirtualListVisibleRows(count, itemSize, h->offset, h->viewport);
}

El* VirtualList::New(Ctx* cx, Str id, const VirtualListOpts& o) {
    Arena* a = cx->a;
    // The layout is where the handle is answered: it learns how many items
    // there are and how much of them is showing, a pending scroll_to_item is
    // applied against that, and the offset is clamped to the list.
    float offset = o.scrollY;
    if (o.handle) {
        VirtualListHandleLayout(o.handle, o.sizes, o.count, o.rowH, o.viewH);
        offset = o.handle->offset;
    }
    // The rows the viewport can show, and a spacer at each end standing in
    // for the ones that were not built — without the second one the list
    // would scroll only as far as the last row it made.
    VirtualRange range =
        o.sizes ? VirtualListVisibleRange(o.sizes, o.count, offset, o.viewH)
                : VirtualListVisibleRows(o.count, o.rowH, offset, o.viewH);
    El* list = Div(a)->FlexCol();
    if (range.first > 0) {
        float before = o.sizes
                           ? VirtualListItemOrigin(o.sizes, o.count, range.first)
                           : (float)range.first * o.rowH;
        list->Child(Div(a)->H(before));
    }
    for (int ix = range.first; ix < range.end; ix++) {
        if (El* built = o.row ? o.row(o.user, cx, ix) : nullptr) {
            list->Child(built);
        }
    }
    if (range.end < o.count) {
        float content = o.sizes ? VirtualListContentSize(o.sizes, o.count)
                                : (float)o.count * o.rowH;
        float built = o.sizes
                          ? VirtualListItemOrigin(o.sizes, o.count, range.end)
                          : (float)range.end * o.rowH;
        list->Child(Div(a)->H(content - built));
    }

    El* e = New(cx, id)->H(o.viewH + o.pad * 2)->ClipY()->ScrollY(offset);
    if (o.pad > 0) {
        e->Pad(o.pad);
    }
    // Both axes: a row wider than the viewport slides under it rather than
    // being cut, which is what the story's Axis: Both asks for.
    e->ClipX()->ScrollX(o.scrollX);
    if (o.axis == ScrollAxis::Vertical) {
        e->HideScrollbarX();
    } else if (o.axis == ScrollAxis::Horizontal) {
        e->HideScrollbarY();
    }
    if (o.scrollId) {
        e->ScrollId(o.scrollId)->OnScroll(o.onScroll);
    }
    return e->Child(list);
}

El* VirtualList::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->Id(id);
}
} // namespace gpui
