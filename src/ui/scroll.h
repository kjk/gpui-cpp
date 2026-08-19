/* Themed scroll — crates/ui/src/scroll */

#include "ui/sizing.h"

namespace gpui {

namespace component {

// ScrollbarAxis: which bars a scroll area has.
enum class ScrollAxis : uint8_t {
    Vertical,
    Horizontal,
    Both
};

struct Scrollable {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    El* child = nullptr;
    Str id = {};
    float scrollY = 0;
    float scrollX = 0;
    float h = 200;
    ScrollAxis axis = ScrollAxis::Vertical;
    // ScrollbarMode. Always is the mode this tree defaults to; Rust's
    // Scrolling fades out after an idle two seconds and is not ported.
    ScrollbarMode mode = ScrollbarMode::Always;
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
