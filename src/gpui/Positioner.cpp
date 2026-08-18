#include "gpui/Positioner.h"

namespace gpui {

static float MaxF(float a, float b) {
    return a > b ? a : b;
}

// resolve_placement. The arms are in Rust's order, which matters: the second
// arm of each side is the flip, and the third is the "neither fits, take the
// roomier one" fallback.
static Placement ResolvePlacement(Rect trigger, float popupW, float popupH,
                                  float viewW, float viewH, float margin,
                                  const Placement* preferred) {
    float rightLimit = MaxF(viewW - margin, margin);
    float bottomLimit = MaxF(viewH - margin, margin);
    float availLeft = MaxF(trigger.x - margin, 0.f);
    float availRight = MaxF(rightLimit - trigger.Right(), 0.f);
    float availAbove = MaxF(trigger.y - margin, 0.f);
    float availBelow = MaxF(bottomLimit - trigger.Bottom(), 0.f);

    if (preferred && *preferred == Placement::Right) {
        if (popupW <= availRight) {
            return Placement::Right;
        }
        if (popupW <= availLeft) {
            return Placement::Left;
        }
        return availRight >= availLeft ? Placement::Right : Placement::Left;
    }
    if (preferred && *preferred == Placement::Left) {
        if (popupW <= availLeft) {
            return Placement::Left;
        }
        if (popupW <= availRight) {
            return Placement::Right;
        }
        return availLeft >= availRight ? Placement::Left : Placement::Right;
    }
    if (preferred && *preferred == Placement::Bottom) {
        if (popupH <= availBelow) {
            return Placement::Bottom;
        }
        if (popupH <= availAbove) {
            return Placement::Top;
        }
        return availBelow >= availAbove ? Placement::Bottom : Placement::Top;
    }
    // Top, and no preference at all: above is the default.
    if (popupH <= availAbove) {
        return Placement::Top;
    }
    if (popupH <= availBelow) {
        return Placement::Bottom;
    }
    return availBelow >= availAbove ? Placement::Bottom : Placement::Top;
}

// side_origin. Align picks the edge along the side; offset is the gap between
// the trigger and the popup.
static Rect SideOrigin(Rect trigger, float popupW, float popupH,
                       Placement placement, PopupAlign align, float offset) {
    float alignedX = trigger.x;
    float alignedY = trigger.y;
    if (align == PopupAlign::Center) {
        alignedX = trigger.CenterX() - popupW * 0.5f;
        alignedY = trigger.CenterY() - popupH * 0.5f;
    } else if (align == PopupAlign::End) {
        alignedX = trigger.Right() - popupW;
        alignedY = trigger.Bottom() - popupH;
    }

    Rect b = {0, 0, popupW, popupH};
    switch (placement) {
        case Placement::Top:
            b.x = alignedX;
            b.y = trigger.y - popupH - offset;
            break;
        case Placement::Bottom:
            b.x = alignedX;
            b.y = trigger.Bottom() + offset;
            break;
        case Placement::Left:
            b.x = trigger.x - popupW - offset;
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
static Rect ClampToViewport(Rect b, float viewW, float viewH, float margin) {
    float rightLimit = MaxF(viewW - margin, margin);
    float bottomLimit = MaxF(viewH - margin, margin);
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

Positioned PositionSide(Rect trigger, float popupW, float popupH, float viewW,
                        float viewH, float margin, const Placement* preferred,
                        PopupAlign align, float offset) {
    Placement placement = ResolvePlacement(trigger, popupW, popupH, viewW,
                                           viewH, margin, preferred);
    Rect b = SideOrigin(trigger, popupW, popupH, placement, align, offset);
    Positioned out;
    out.bounds = ClampToViewport(b, viewW, viewH, margin);
    out.placement = placement;
    out.hasPlacement = true;
    return out;
}

Positioned PositionCorner(Anchor anchor, float x, float y, float popupW,
                          float popupH, float viewW, float viewH,
                          float margin) {
    // Bounds::from_anchor_and_size.
    Rect b = {x, y, popupW, popupH};
    if (anchor == Anchor::TopRight || anchor == Anchor::BottomRight) {
        b.x = x - popupW;
    }
    if (anchor == Anchor::BottomLeft || anchor == Anchor::BottomRight) {
        b.y = y - popupH;
    }
    Positioned out;
    out.bounds = ClampToViewport(b, viewW, viewH, margin);
    out.hasPlacement = false;
    return out;
}

} // namespace gpui
