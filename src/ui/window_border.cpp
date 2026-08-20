#include "ui/window_border.h"

namespace gpui {

namespace component {

Edges WindowBorderInsets(float shadowSize, WindowTiling tiling) {
    Edges e = {shadowSize, shadowSize, shadowSize, shadowSize};
    if (tiling.top) {
        e.top = 0;
    }
    if (tiling.bottom) {
        e.bottom = 0;
    }
    if (tiling.left) {
        e.left = 0;
    }
    if (tiling.right) {
        e.right = 0;
    }
    return e;
}

WindowEdge WindowResizeEdge(float x, float y, float w, float h, Edges insets,
                            WindowTiling tiling, float hitSize) {
    float innerLeft = insets.left;
    float innerRight = w - insets.right;
    float innerTop = insets.top;
    float innerBottom = h - insets.bottom;

    // Each edge only counts along its own segment of the inner frame: it does
    // not run on down the extension lines of the shadow padding.
    bool onLeft = x >= innerLeft - hitSize && x <= innerLeft + hitSize &&
                  y >= innerTop - hitSize && y <= innerBottom + hitSize;
    bool onRight = x >= innerRight - hitSize && x <= innerRight + hitSize &&
                   y >= innerTop - hitSize && y <= innerBottom + hitSize;
    bool onTop = y >= innerTop - hitSize && y <= innerTop + hitSize &&
                 x >= innerLeft - hitSize && x <= innerRight + hitSize;
    bool onBottom = y >= innerBottom - hitSize && y <= innerBottom + hitSize &&
                    x >= innerLeft - hitSize && x <= innerRight + hitSize;

    if (!tiling.top && !tiling.left && onTop && onLeft) {
        return WindowEdge::TopLeft;
    }
    if (!tiling.top && !tiling.right && onTop && onRight) {
        return WindowEdge::TopRight;
    }
    if (!tiling.bottom && !tiling.left && onBottom && onLeft) {
        return WindowEdge::BottomLeft;
    }
    if (!tiling.bottom && !tiling.right && onBottom && onRight) {
        return WindowEdge::BottomRight;
    }
    if (!tiling.top && onTop) {
        return WindowEdge::Top;
    }
    if (!tiling.bottom && onBottom) {
        return WindowEdge::Bottom;
    }
    if (!tiling.left && onLeft) {
        return WindowEdge::Left;
    }
    if (!tiling.right && onRight) {
        return WindowEdge::Right;
    }
    return WindowEdge::None;
}

WindowBorder* WindowBorder::New(Ctx* cx) {
    Arena* a = cx->a;
    WindowBorder* w = ArenaNew<WindowBorder>(a);
    w->a = a;
    w->cx = cx;
    return w;
}
WindowBorder* WindowBorder::Child(El* e) {
    child = e;
    return this;
}
WindowBorder* WindowBorder::ShadowSize(float v) {
    shadowSize = v;
    return this;
}
WindowBorder* WindowBorder::Tiling(WindowTiling v) {
    tiling = v;
    return this;
}

El* WindowBorder::IntoEl() {
    const Theme& th = cx->theme();
    // A window tiled on every side keeps its platform inset but draws no
    // shadow: there is nothing for one to fall on.
    float visualShadow = tiling.AllTiled() ? 0.f : shadowSize;
    Edges insets = WindowBorderInsets(visualShadow, tiling);

    El* backdrop = Div(a)->FlexCol()->SizeFull()->ClipY();
    if (insets.top > 0) {
        backdrop->PadT(insets.top);
    }
    if (insets.bottom > 0) {
        backdrop->PadB(insets.bottom);
    }
    if (insets.left > 0) {
        backdrop->PadL(insets.left);
    }
    if (insets.right > 0) {
        backdrop->PadR(insets.right);
    }

    // The frame itself: a one-pixel border on every side the window is not
    // tiled against, dimmed while the window is not the active one.
    El* frame = Div(a)->FlexCol()->Grow()->MinH(0)->W(kFill)->ClipY();
    Rgba border = th.border;
    if (!WindowIsActive(cx)) {
        border = RgbaOpacity(border, 0.7f);
    }
    if (!tiling.top) {
        frame->BorderT(kWindowBorderSize, border);
    }
    if (!tiling.bottom) {
        frame->BorderB(kWindowBorderSize, border);
    }
    if (!tiling.left) {
        frame->BorderL(kWindowBorderSize, border);
    }
    if (!tiling.right) {
        frame->BorderR(kWindowBorderSize, border);
    }
    if (child) {
        frame->Child(child);
    }
    backdrop->Child(frame);
    return backdrop;
}

} // namespace component
} // namespace gpui
