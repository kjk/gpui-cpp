/* Placement, Side and the axis helper — crates/base/src/geometry.rs
 *
 * The small geometry vocabulary the base layer adds to gpui's own. Rust
 * writes `AxisExt` and `LengthExt` as traits on `gpui::Axis` and
 * `gpui::Length`, which is the only way to hang a method on another
 * crate's type; here they are free functions on the same enums. gpui's
 * `Point`, `Size`, `Bounds` and `Edges` are in `gpui/gpui.h`, where the
 * crate that owns them is. `LengthExt::to_pixels` has no counterpart: a
 * length here is resolved by taffy, not by the caller. */

#include "gpui/gpui.h"

namespace gpui {

// The side of the trigger the popup is placed on.
enum class Placement : uint8_t {
    Top,
    Bottom,
    Left,
    Right
};

// Which edge of the window something hangs off.
enum class Side : uint8_t {
    Left,
    Right
};

inline bool SideIsLeft(Side s) {
    return s == Side::Left;
}

inline bool AxisIsHorizontal(Axis a) {
    return a == Axis::Horizontal;
}

} // namespace gpui
