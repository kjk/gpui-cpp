
#include "gpui/gpui.h"

// Lucide-style SVG (viewBox, path/rect/polyline/line/circle/polygon).
// Stroke uses `color`; fill="none" icons are stroked only.

namespace gpui {

// `turns` rotates the icon clockwise about the middle of its box, 1 being a
// whole turn — Transformation::rotate(percentage(..)), applied as the path is
// built rather than by the backend.
// The viewBox of an asset, so a caller that knows one dimension can work out
// the other. False when the file is not there or is not an SVG this reader
// understands.
bool SvgViewBox(Str assetPath, Size* out);

bool SvgDraw(PaintCtx* ctx, Str assetPath, float x, float y, float size,
             Rgba color, float turns = 0);

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
