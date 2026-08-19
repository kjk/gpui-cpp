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

El* VirtualList::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->Id(id);
}
} // namespace gpui
