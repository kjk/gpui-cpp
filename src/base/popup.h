/* Unstyled popup — crates/base/src/popup.rs */

#include "gpui/gpui.h"

namespace gpui {

// Which corner of the trigger the content hangs off. GPUI's Anchor, whole:
// the two centre-of-side entries exist in the enum but resolve to the origin,
// because a popup that anchors sideways is positioned by the positioner
// rather than by a corner.
enum class PopupAnchor : uint8_t {
    TopLeft,
    TopCenter,
    TopRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
    LeftCenter,
    RightCenter
};

// resolved_corner: the point on the trigger the content is placed against.
// The bottom anchors take the trigger's bottom edge, so content hanging below
// starts where the trigger ends rather than where it began.
Point PopupResolvedCorner(PopupAnchor anchor, Bounds triggerBounds);

struct Popup {
    El* root = nullptr;
    // Where the content hangs. Rust defaults to TopLeft, and so does this.
    PopupAnchor anchor = PopupAnchor::TopLeft;

    static Popup* New(Ctx* cx, Str id, El* trigger,
                      PopupAnchor anchor = PopupAnchor::TopLeft);
    Popup* Anchor(PopupAnchor a);
    // The older spelling, kept for the pages that only need the right edge
    // lined up: Anchor(TopRight).
    Popup* AnchorRight(bool on = true);
    Popup* Content(El* content);
    El* IntoEl();
};
} // namespace gpui
