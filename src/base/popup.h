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

// Where a popup's content hangs off its trigger, as the insets PlaceAnchored
// reads: the Top anchors put it under the trigger and the Bottom ones over
// it, and the second half of the name is the edge the two line up on. Every
// anchored surface — the Popup, the Popover, the HoverCard — goes through
// this so the six corners cannot drift apart between them. `Fixed` is what
// makes the content lay out against the window the way Rust's Positioner
// does, rather than inside the trigger.
El* PopupPlaceContent(El* content, PopupAnchor anchor, float gap);

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
