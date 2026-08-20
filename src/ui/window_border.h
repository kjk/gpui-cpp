/* Window border — crates/ui/src/window_border.rs

   A client-decorated window draws its own frame: a band of shadow padding
   around it, a one-pixel border inside that, and a resize hit band along the
   inner frame. A server-decorated one — which is what Windows and macOS give
   us — draws none of it, and `WindowBorderInsets` answers zero. */

#include "ui/sizing.h"

namespace gpui {

namespace component {

// SHADOW_SIZE: the padding a client-decorated window keeps around its frame
// for the shadow. Zero on the platforms whose window manager draws one.
#if GPUI_OS_LINUX
const float kWindowShadowSize = 20;
#else
const float kWindowShadowSize = 0;
#endif
const float kWindowBorderSize = 1;
// Half the width of the band a press counts as a resize, either side of the
// visible frame.
const float kWindowResizeHitSize = 4;
// GPUI clips children to a rectangular mask, so a rounded frame would leave
// the corners of a child's background showing. Square until that changes,
// which is what Rust says here too.
const float kWindowBorderRadius = 0;

// Tiling: which sides the window manager has snapped flush against something.
// A tiled side has no shadow, no border and no resize band.
struct WindowTiling {
    bool top = false;
    bool bottom = false;
    bool left = false;
    bool right = false;

    bool IsTiled() const { return top || bottom || left || right; }
    bool AllTiled() const { return top && bottom && left && right; }
};

// client_frame_insets: how far the visible frame sits inside the window, per
// side.
Edges WindowBorderInsets(float shadowSize, WindowTiling tiling);

// ResizeEdge, numbered as _NET_WM_MOVERESIZE numbers its directions —
// clockwise from the top-left corner — because that is what the X11 window
// sends when one of them is grabbed. None is -1.
enum class WindowEdge : int8_t {
    None = -1,
    TopLeft = 0,
    Top = 1,
    TopRight = 2,
    Right = 3,
    BottomRight = 4,
    Bottom = 5,
    BottomLeft = 6,
    Left = 7
};

// resize_edge: which edge a press at (x, y) grabs, or None. Each edge only
// counts along its own segment of the inner frame — it does not run on down
// the shadow padding past the corner — and a tiled side is never grabbed.
WindowEdge WindowResizeEdge(float x, float y, float w, float h, Edges insets,
                            WindowTiling tiling, float hitSize);

struct WindowBorder {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    El* child = nullptr;
    float shadowSize = kWindowShadowSize;
    WindowTiling tiling = {};

    static WindowBorder* New(Ctx* cx);
    WindowBorder* Child(El* e);
    WindowBorder* ShadowSize(float v);
    WindowBorder* Tiling(WindowTiling v);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
