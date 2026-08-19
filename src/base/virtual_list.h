/* Unstyled virtual list host — crates/base/src/virtual_list.rs */

#include "gpui/gpui.h"

namespace gpui {

// Which items a scrolled list has to build. Everything outside this range is
// not rendered at all, which is the whole point of a virtual list: a
// half-million-row list costs a screenful.
//
// `sizes` is the length of each item along the scrolled axis — Rust takes a
// Size per item and reads only the one the axis names. `offset` is how far the
// list has scrolled, positive-down as El::ScrollY takes it; Rust's is negative
// because it offsets the content rather than the view.
//
// The end is exclusive and runs past what is strictly visible: the search
// stops at the first item whose end is below the bottom edge, takes that one
// in, and Rust then adds one more — so the row being scrolled into is already
// built when it arrives. When nothing crossed the bottom edge the rest of the
// list fits and all of it is taken.
struct VirtualRange {
    int first = 0;
    int end = 0;
};

VirtualRange VirtualListVisibleRange(const float* sizes, int count,
                                     float offset, float viewport);

// The distance from the start of the list to item `ix` — Rust's running scan
// over the sizes, which is where each item is placed.
float VirtualListItemOrigin(const float* sizes, int count, int ix);

// Every item's length added up: the scrolled size of the whole list.
float VirtualListContentSize(const float* sizes, int count);

struct VirtualList {
    static El* New(Ctx* cx, Str id);
};
} // namespace gpui
