#include "gpui/Positioner.h"

namespace gpui {

static float MaxF(float a, float b) {
    return a > b ? a : b;
}

// resolve_placement. The arms are in Rust's order, which matters: the second
// arm of each side is the flip, and the third is the "neither fits, take the
// roomier one" fallback.
static Placement ResolvePlacement(Rect trigger, Size popup, Size view,
                                  float margin, const Placement* preferred) {
    float rightLimit = MaxF(view.w - margin, margin);
    float bottomLimit = MaxF(view.h - margin, margin);
    float availLeft = MaxF(trigger.x - margin, 0.f);
    float availRight = MaxF(rightLimit - trigger.Right(), 0.f);
    float availAbove = MaxF(trigger.y - margin, 0.f);
    float availBelow = MaxF(bottomLimit - trigger.Bottom(), 0.f);

    if (preferred && *preferred == Placement::Right) {
        if (popup.w <= availRight) {
            return Placement::Right;
        }
        if (popup.w <= availLeft) {
            return Placement::Left;
        }
        return availRight >= availLeft ? Placement::Right : Placement::Left;
    }
    if (preferred && *preferred == Placement::Left) {
        if (popup.w <= availLeft) {
            return Placement::Left;
        }
        if (popup.w <= availRight) {
            return Placement::Right;
        }
        return availLeft >= availRight ? Placement::Left : Placement::Right;
    }
    if (preferred && *preferred == Placement::Bottom) {
        if (popup.h <= availBelow) {
            return Placement::Bottom;
        }
        if (popup.h <= availAbove) {
            return Placement::Top;
        }
        return availBelow >= availAbove ? Placement::Bottom : Placement::Top;
    }
    // Top, and no preference at all: above is the default.
    if (popup.h <= availAbove) {
        return Placement::Top;
    }
    if (popup.h <= availBelow) {
        return Placement::Bottom;
    }
    return availBelow >= availAbove ? Placement::Bottom : Placement::Top;
}

// side_origin. Align picks the edge along the side; offset is the gap between
// the trigger and the popup.
static Rect SideOrigin(Rect trigger, Size popup, Placement placement,
                       PopupAlign align, float offset) {
    float alignedX = trigger.x;
    float alignedY = trigger.y;
    if (align == PopupAlign::Center) {
        alignedX = trigger.CenterX() - popup.w * 0.5f;
        alignedY = trigger.CenterY() - popup.h * 0.5f;
    } else if (align == PopupAlign::End) {
        alignedX = trigger.Right() - popup.w;
        alignedY = trigger.Bottom() - popup.h;
    }

    Rect b = {0, 0, popup.w, popup.h};
    switch (placement) {
        case Placement::Top:
            b.x = alignedX;
            b.y = trigger.y - popup.h - offset;
            break;
        case Placement::Bottom:
            b.x = alignedX;
            b.y = trigger.Bottom() + offset;
            break;
        case Placement::Left:
            b.x = trigger.x - popup.w - offset;
            b.y = alignedY;
            break;
        case Placement::Right:
            b.x = trigger.Right() + offset;
            b.y = alignedY;
            break;
    }
    return b;
}

// clamp. The far edge is pulled in first and the near edge second, so a popup
// larger than the viewport ends up flush against the near edge rather than
// hanging off both.
static Rect ClampToViewport(Rect b, Size view, float margin) {
    float rightLimit = MaxF(view.w - margin, margin);
    float bottomLimit = MaxF(view.h - margin, margin);
    if (b.Right() > rightLimit) {
        b.x -= b.Right() - rightLimit;
    }
    if (b.x < margin) {
        b.x = margin;
    }
    if (b.Bottom() > bottomLimit) {
        b.y -= b.Bottom() - bottomLimit;
    }
    if (b.y < margin) {
        b.y = margin;
    }
    return b;
}

Positioned PositionSide(Rect trigger, Size popup, Size view, float margin,
                        const Placement* preferred, PopupAlign align,
                        float offset) {
    Placement placement =
        ResolvePlacement(trigger, popup, view, margin, preferred);
    Rect b = SideOrigin(trigger, popup, placement, align, offset);
    Positioned out;
    out.bounds = ClampToViewport(b, view, margin);
    out.placement = placement;
    out.hasPlacement = true;
    return out;
}

Positioned PositionCorner(Anchor anchor, Point at, Size popup, Size view,
                          float margin) {
    // Bounds::from_anchor_and_size.
    Rect b = RectAt(at, popup);
    if (anchor == Anchor::TopRight || anchor == Anchor::BottomRight) {
        b.x = at.x - popup.w;
    }
    if (anchor == Anchor::BottomLeft || anchor == Anchor::BottomRight) {
        b.y = at.y - popup.h;
    }
    Positioned out;
    out.bounds = ClampToViewport(b, view, margin);
    out.hasPlacement = false;
    return out;
}

} // namespace gpui
