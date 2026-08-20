
#include "gpui/gpui.h"

// Lucide-style SVG (viewBox, path/rect/polyline/line/circle/polygon).
// Stroke uses `color`; fill="none" icons are stroked only.

namespace gpui {

bool SvgDraw(PaintCtx* ctx, Str assetPath, float x, float y, float size,
             Rgba color);

// The same icon, drawn into a square of pixels instead of onto a window:
// `outBgra` takes px * px * 4 bytes of premultiplied BGRA, top down. What a
// menu the OS draws needs, since it wants a bitmap of the icon rather than
// something that can draw one.
bool SvgRasterize(PaintApp* pa, Str assetPath, int px, Rgba color,
                  uint8_t* outBgra);

// The same icon, drawn into a square of pixels instead of onto a window:
// `outBgra` takes px * px * 4 bytes of premultiplied BGRA, top down. What a
// menu the OS draws needs, since it wants a bitmap of the icon rather than
// something that can draw one.
bool SvgRasterize(PaintApp* pa, Str assetPath, int px, Rgba color,
                  uint8_t* outBgra);

// IconName -> "icons/<kebab>.svg" (same mapping as gpui-component's
// icon_named!).
Str IconNamePath(IconName name);
} // namespace gpui
