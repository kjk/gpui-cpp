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

// The same range for a list whose items are all one size — uniform_list,
// which is what a tree renders through. Worked out by division rather than by
// scanning, so a hundred thousand rows cost nothing to skip.
VirtualRange VirtualListVisibleRows(int count, float rowSize, float offset,
                                    float viewport);

// The distance from the start of the list to item `ix` — Rust's running scan
// over the sizes, which is where each item is placed.
float VirtualListItemOrigin(const float* sizes, int count, int ix);

// gpui::ScrollStrategy, which says where scroll_to_item leaves the item it
// brought into view. The virtual list treats Top and Bottom alike — Rust
// matches only Center and lets the rest fall into one branch that scrolls as
// little as it can: an item above the view aligns with the top, one below
// aligns with the bottom, and one already in view does not move at all.
enum class ScrollStrategy : uint8_t {
    Top,
    Center,
    Bottom
};

// scroll_to_item: the offset that brings the item at `origin` into a viewport
// of `viewport`, from where it is now. Offsets run positive-down, as
// El::ScrollY takes them, and the answer is clamped to the list — there is
// nothing to see past either end.
float VirtualListScrollTo(float origin, float size, float offset,
                          float viewport, float contentSize,
                          ScrollStrategy strategy);
// The same, for the item at `ix` of a list whose items are `sizes`.
float VirtualListScrollToItem(const float* sizes, int count, int ix,
                              float offset, float viewport,
                              ScrollStrategy strategy);
// The same again, for a list whose items are all one size — which is what
// uniform_list is, and what the tree renders through.
float VirtualListScrollToRow(int count, float rowSize, int ix, float offset,
                             float viewport, ScrollStrategy strategy);

// Every item's length added up: the scrolled size of the whole list.
float VirtualListContentSize(const float* sizes, int count);

// VirtualListScrollHandle. Rust's handle is what a caller outside the list
// holds: it knows the axis and how many items there are, it carries the
// viewport and the offset of the list it is attached to, and a request to
// scroll to an item waits on it until the list is next laid out — at the
// moment the request is made nothing knows where the item is.
struct VirtualListScrollHandle {
    Axis axis = Axis::Vertical;
    int itemsCount = 0;
    // The base handle: where the list has scrolled to, how much of it is
    // showing, and how long the whole of it is. Offsets run positive-down,
    // as El::ScrollY takes them.
    float offset = 0;
    float viewport = 0;
    float contentSize = 0;
    // deferred_scroll_to_item: the request, until a layout can answer it.
    bool pending = false;
    int pendingIx = 0;
    // scroll_to_item_with_offset: how many items past the one named to bring
    // into view instead.
    int pendingOffset = 0;
    ScrollStrategy pendingStrategy = ScrollStrategy::Top;
};

// scroll_to_item / scroll_to_item_with_offset: the request is recorded and
// answered at the next layout.
void VirtualListScrollToItemDeferred(VirtualListScrollHandle* h, int ix,
                                     ScrollStrategy strategy);
void VirtualListScrollToItemDeferredWithOffset(VirtualListScrollHandle* h,
                                               int ix, ScrollStrategy strategy,
                                               int offset);
// scroll_to_bottom: the last item, at the top of the view — which is as far
// as the list goes.
void VirtualListScrollToBottomDeferred(VirtualListScrollHandle* h);

// What the list does with the handle when it lays out: the item sizes and the
// viewport it measured, then the pending request if there is one, and last
// the clamp that keeps the offset inside the list. `sizes` may be null for a
// list whose items are all `itemSize` long. Answers whether the offset moved.
bool VirtualListHandleLayout(VirtualListScrollHandle* h, const float* sizes,
                             int count, float itemSize, float viewport);

// The range the handle's offset makes visible, for the same pair of lists.
VirtualRange VirtualListHandleRange(const VirtualListScrollHandle* h,
                                    const float* sizes, int count,
                                    float itemSize);

struct VirtualList {
    static El* New(Ctx* cx, Str id);
};
} // namespace gpui
