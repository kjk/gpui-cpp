#ifndef GPUI_SRC_UI_SCROLL_H_
#define GPUI_SRC_UI_SCROLL_H_
/* Themed scroll — crates/ui/src/scroll */

#include "ui/sizing.h"

namespace gpui {

namespace component {

// ScrollbarAxis. The axis belongs to the box that scrolls, which is
// `base/scrollbar.h` — this is the same enum under the name the themed side
// has always used it by.
using ScrollbarAxis = gpui::ScrollAxis;
using ScrollAxis = ScrollbarAxis; // compatibility with the original C++ API

struct Scrollable {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    // The source element remains the viewport. The Rust implementation needs
    // an outer layout wrapper because its Scrollbar is a sibling element; the
    // renderer owns the bar here, so that wrapper collapses away.
    El* element = nullptr;
    Str id = {};
    float scrollY = 0;
    float scrollX = 0;
    float h = 0;
    bool hSet = false;
    ScrollAxis axis = ScrollAxis::Vertical;
    // No explicit mode reads the projected Base theme (Scrolling by default).
    ScrollbarMode mode = ScrollbarMode::Scrolling;
    bool modeSet = false;
    // Where a scrollbar press, a drag or the wheel reports the offset it
    // worked out. The view owns the offsets, so it is the one that stores
    // them.
    Listener onScroll;

    static Scrollable* New(Ctx* cx);
    static Scrollable* New(Ctx* cx, Str id);
    static Scrollable* New(Ctx* cx, El* element,
                           ScrollAxis axis = ScrollAxis::Both);
    Scrollable* Id(Str v);
    Scrollable* Child(El* e);
    Scrollable* ScrollY(float v);
    Scrollable* ScrollX(float v);
    Scrollable* Axis(ScrollAxis v);
    Scrollable* Mode(ScrollbarMode v);
    Scrollable* H(float v);
    Scrollable* OnScroll(Listener fn);
    El* IntoEl();
};

// crates/ui/src/scroll/scrollable.rs::ScrollableElement. Rust expresses this
// as an extension trait over Div/Stateful; C++ makes the same transformations
// explicit and returns the fluent Scrollable builder.
struct ScrollableElement {
    static El* Scrollbar(Ctx* cx, El* element, Str id, float scrollY,
                         float scrollX, Listener onScroll,
                         ScrollbarAxis axis = ScrollbarAxis::Vertical);
    static El* VerticalScrollbar(Ctx* cx, El* element, Str id, float scrollY,
                                 Listener onScroll);
    static El* HorizontalScrollbar(Ctx* cx, El* element, Str id, float scrollX,
                                   Listener onScroll);
    static Scrollable* OverflowScrollbar(Ctx* cx, El* element);
    static Scrollable* OverflowXScrollbar(Ctx* cx, El* element);
    static Scrollable* OverflowYScrollbar(Ctx* cx, El* element);
};

// crates/ui/src/scroll/scrollable_mask.rs::ScrollableMask. GPUI paints this
// as a transparent sibling over a ScrollHandle. The integrated C++ scroll
// element already owns that handle, so IntoEl returns the same viewport after
// marking the axis whose wheel gestures its mask captures.
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

} // namespace component
} // namespace gpui
#endif // GPUI_SRC_UI_SCROLL_H_
