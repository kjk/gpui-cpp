#ifndef GPUI_BASE_SCROLLABLE_MASK_H_
#define GPUI_BASE_SCROLLABLE_MASK_H_
/* The wheel mask over a scroll viewport — crates/base/src/scrollable_mask.rs

   It moved down from `crates/ui/src/scroll/` when the Markdown table came to
   gpui-base: everything it needs — the scrollbar handle, the axis vocabulary,
   the ongoing-scroll axis lock — was already here, and node.rs is what wants
   it. `src/ui/scroll.h` re-exports it, so `component::ScrollableMask` still
   names this type.

   GPUI paints the mask as a transparent sibling over a ScrollHandle and takes
   the wheel in the *capture* phase, so an ancestor scroller — `gpui::list`
   under a scrollable TextView — cannot consume the vertical half of a
   diagonal swipe first. The integrated C++ scroll element owns its handle, so
   the sibling collapses onto the viewport itself: `Apply` marks the axis
   whose gestures the viewport traps, and `window_common.cpp` runs the same
   per-gesture axis lock and edge rule the Rust element does — a vertical mask
   hands the event to the ancestor at its edge, a horizontal one keeps it. */

#include "gpui/gpui.h"

namespace gpui {

struct ScrollableMask {
    Arena* a = nullptr;
    Axis axis = Axis::Vertical;
    El* element = nullptr;
    Str id = {};
    bool debug = false;

    static ScrollableMask* New(Ctx* cx, Axis axis, El* element);
    static El* Apply(El* element, Axis axis);
    ScrollableMask* Id(Str v);
    ScrollableMask* Debug(bool v = true);
    El* IntoEl();
};

// `horizontal_scroll_area(id, handle, style, child)`: a viewport that clips
// and scrolls sideways with a horizontal mask over it, so a vertical wheel
// keeps bubbling to the document. `style` is the refinement the frame — its
// background, border and radius — is painted from.
El* HorizontalScrollArea(Ctx* cx, Str id, El* viewport);

} // namespace gpui
#endif // GPUI_BASE_SCROLLABLE_MASK_H_
