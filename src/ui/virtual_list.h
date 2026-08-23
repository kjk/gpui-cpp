/* Themed virtual list — crates/ui/src/virtual_list.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

struct VirtualList {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = StrL("vlist");
    int count = 0;
    float rowH = 32;
    float viewH = 192;
    float scrollY = 0;
    // The item sizes, for a list whose rows are not all one height. Null is
    // the uniform list, which is `rowH` per row.
    const float* sizes = nullptr;
    // The handle the caller holds: where the list has scrolled to, and where
    // it has been asked to scroll. A list with one reads its offset from the
    // handle rather than from `scrollY`.
    VirtualListScrollHandle* handle = nullptr;
    Listener onRenderRow; // not used; rows built here
    El* (*row)(Ctx* cx, int ix) = nullptr;
    // The scroll id and the listener a scrolled list needs to hear the wheel.
    int scrollId = 0;
    Listener onScroll = {};

    static VirtualList* New(Ctx* cx, int count);
    VirtualList* Id(Str v);
    VirtualList* RowH(float v);
    VirtualList* ViewH(float v);
    VirtualList* ScrollY(float v);
    VirtualList* Sizes(const float* v);
    VirtualList* Handle(VirtualListScrollHandle* h);
    VirtualList* Scroll(int id, Listener onScroll);
    // The row builder. Rust's is a closure that captured `cx`, so this takes
    // one: a row that reads the theme has no other way to.
    VirtualList* Row(El* (*fn)(Ctx*, int));
    El* IntoEl();
};

} // namespace component
} // namespace gpui
