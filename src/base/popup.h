/* Unstyled popup — crates/base/src/popup.rs */

#include "gpui/gpui.h"

namespace gpui {

// Rust imports gpui::Anchor directly. Keep the older port spelling as an
// alias so existing examples do not need a flag-day rename.
using PopupAnchor = Anchor;

// popup.rs's two concrete overlay constants. Deferred priorities are mapped
// to the runtime's paint layers, but the public value remains available for
// components that compare or forward it.
constexpr int kPopupPriority = 100;
constexpr float kPopupWindowMargin = 8.f;

// resolved_corner: the exact point popup.rs feeds Positioner::corner. Top
// anchors use the trigger origin; Bottom anchors subtract its height.
Point PopupResolvedCorner(PopupAnchor anchor, Bounds triggerBounds);

// Place `anchor`'s point on content at PopupResolvedCorner(anchor, trigger),
// clamp it eight pixels inside the window, and defer its paint. Every Popup-
// backed surface goes through this so the eight anchors cannot drift apart.
El* PopupPlaceContent(El* content, PopupAnchor anchor, float offsetY = 0);

struct Popup {
    El* root = nullptr;
    // Where the content hangs. Rust defaults to TopLeft, and so does this.
    PopupAnchor anchor = PopupAnchor::TopLeft;
    // Rust withholds deferred content until the trigger's first prepaint has
    // captured bounds. This runtime can place from live layout, but keeps the
    // same first-frame visibility contract.
    bool contentReady = false;

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
