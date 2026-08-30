#include "base/popup.h"

namespace gpui {

struct PopupAnchorState {
    bool captured = false;
};

Point PopupResolvedCorner(PopupAnchor anchor, Bounds b) {
    switch (anchor) {
        case PopupAnchor::TopLeft:
            return {b.x, b.y};
        case PopupAnchor::TopCenter:
            return {b.CenterX(), b.y};
        case PopupAnchor::TopRight:
            return {b.Right(), b.y};
        case PopupAnchor::BottomLeft:
            return {b.x, b.y - b.h};
        case PopupAnchor::BottomCenter:
            return {b.CenterX(), b.y - b.h};
        case PopupAnchor::BottomRight:
            return {b.Right(), b.y - b.h};
        default:
            // LeftCenter and RightCenter: Rust hands back the origin, since a
            // popup anchored sideways is placed by the positioner instead.
            return {b.x, b.y};
    }
}

El* PopupPlaceContent(El* content, PopupAnchor anchor, float offsetY) {
    if (!content) {
        return content;
    }
    // Positioner::corner with Popup's own 8 px viewport margin. The runtime
    // can measure the trigger and child in one layout pass, so it does not
    // need Rust's first-frame prepaint capture; the resulting bounds are the
    // same. Deferred is the structural equivalent of with_priority(100).
    return content->AnchorCorner(anchor, kPopupWindowMargin, offsetY)
        ->Deferred();
}

Popup* Popup::New(Ctx* cx, Str id, El* trigger, PopupAnchor anchor) {
    Arena* a = cx->a;
    Popup* p = ArenaNew<Popup>(a);
    p->anchor = anchor;
    PopupAnchorState* state =
        ElementState<PopupAnchorState>(cx, id, StrL("PopupAnchorState"));
    p->contentReady = state && state->captured;
    if (state && !state->captured) {
        state->captured = true;
        WindowRequestAnimationFrame(cx->win);
    }
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
    if (!content || !contentReady) {
        return this;
    }
    // Positioner::corner is out of flow and deferred; in-flow content would
    // grow its trigger's page and paint below later siblings.
    if (!content->style.absolute) {
        PopupPlaceContent(content, anchor);
    }
    root->Child(content);
    return this;
}

El* Popup::IntoEl() {
    return root;
}
} // namespace gpui
