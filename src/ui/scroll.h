/* Themed scroll — crates/ui/src/scroll */

#include "ui/sizing.h"

namespace gpui {

namespace component {

// ScrollbarAxis. The axis belongs to the box that scrolls, which is
// `base/scrollbar.h` — this is the same enum under the name the themed side
// has always used it by.
using ScrollAxis = gpui::ScrollAxis;

struct Scrollable {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    El* child = nullptr;
    Str id = {};
    float scrollY = 0;
    float scrollX = 0;
    float h = 200;
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
    Scrollable* Child(El* e);
    Scrollable* ScrollY(float v);
    Scrollable* ScrollX(float v);
    Scrollable* Axis(ScrollAxis v);
    Scrollable* Mode(ScrollbarMode v);
    Scrollable* H(float v);
    Scrollable* OnScroll(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
