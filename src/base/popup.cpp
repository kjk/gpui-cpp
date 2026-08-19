#include "base/popup.h"

namespace gpui {

Point PopupResolvedCorner(PopupAnchor anchor, Bounds b) {
    switch (anchor) {
        case PopupAnchor::TopLeft:
            return {b.x, b.y};
        case PopupAnchor::TopCenter:
            return {b.CenterX(), b.y};
        case PopupAnchor::TopRight:
            return {b.Right(), b.y};
        case PopupAnchor::BottomLeft:
            return {b.x, b.Bottom()};
        case PopupAnchor::BottomCenter:
            return {b.CenterX(), b.Bottom()};
        case PopupAnchor::BottomRight:
            return {b.Right(), b.Bottom()};
        default:
            // LeftCenter and RightCenter: Rust hands back the origin, since a
            // popup anchored sideways is placed by the positioner instead.
            return {b.x, b.y};
    }
}

Popup* Popup::New(Ctx* cx, Str id, El* trigger, PopupAnchor anchor) {
    Arena* a = cx->a;
    Popup* p = ArenaNew<Popup>(a);
    p->anchor = anchor;
    // Root sizes to the trigger only. Content is an overlay (Rust Positioner).
    p->root = Div(a)->Id(id);
    if (trigger) {
        p->root->Child(trigger);
    }
    return p;
}

Popup* Popup::Anchor(PopupAnchor a) {
    anchor = a;
    return this;
}

Popup* Popup::AnchorRight(bool on) {
    anchor = on ? PopupAnchor::TopRight : PopupAnchor::TopLeft;
    return this;
}

Popup* Popup::Content(El* content) {
    if (!content) {
        return this;
    }
    // Sit under the trigger. In-flow content would grow the centered
    // showcase page and jump the trigger; overlaying covers it so a second
    // click cannot dismiss.
    if (!content->style.absolute) {
        content->AnchorBelow(4);
        // The corner the anchor names, expressed as the edge the content is
        // pinned to: the two right-hand anchors line its right edge up with
        // the trigger's, the centre ones centre it, and the rest hang left.
        switch (anchor) {
            case PopupAnchor::TopRight:
            case PopupAnchor::BottomRight:
                content->Right(0);
                break;
            case PopupAnchor::TopCenter:
            case PopupAnchor::BottomCenter:
                content->AnchorCenterX();
                break;
            default:
                content->Left(0);
                break;
        }
    }
    // Rust puts popover content in a deferred layer, so it draws over
    // whatever follows the trigger in the tree rather than under it.
    content->Deferred();
    root->Child(content);
    return this;
}

El* Popup::IntoEl() {
    return root;
}
} // namespace gpui
