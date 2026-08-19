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

El* VirtualList::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->Id(id);
}
} // namespace gpui
