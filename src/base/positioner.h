/* Popup positioning — crates/base/src/positioner.rs
 *
 * Every anchored surface resolves its position here, so flipping, alignment
 * and viewport clamping cannot drift apart between popups, tooltips and menus.
 * The functions are pure: they take the trigger, the popup's measured size and
 * the viewport, and hand back where the popup goes. Nothing here paints.
 */

#include "gpui/gpui.h"

namespace gpui {

// The side of the trigger the popup is placed on.
enum class Placement : uint8_t {
    Top,
    Bottom,
    Left,
    Right
};

// Where the popup sits along the side it was placed on.
enum class PopupAlign : uint8_t {
    Start,
    Center,
    End
};

// The corner of the popup that lands on the requested position. GPUI's
// `anchored` corner behavior, which clamps but never flips.
enum class Anchor : uint8_t {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight
};

// Where the popup ended up, and the side it took. `hasPlacement` is false for
// corner positioning, which has no notion of a side.
struct Positioned {
    Bounds bounds;
    Placement placement = Placement::Top;
    bool hasPlacement = false;
};

// WINDOW_MARGIN: the distance kept between a popup and the viewport edge.
constexpr float kPopupMargin = 4.f;

// Places the popup on a side of the trigger. It takes the preferred side when
// the popup fits there, otherwise the opposite side, otherwise whichever side
// has more room; the result is then clamped into the viewport. `preferred` is
// null for Rust's `None`, which prefers Top.
Positioned PositionSide(Bounds trigger, Size popup, Size view, float margin,
                        const Placement* preferred, PopupAlign align,
                        float offset);

// Puts `anchor`'s corner of the popup at `at`, then clamps into the
// viewport. Never flips, and never reports a side.
Positioned PositionCorner(Anchor anchor, Point at, Size popup, Size view,
                          float margin);

} // namespace gpui
