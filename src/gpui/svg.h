
#include "gpui/gpui.h"

// Lucide-style SVG (viewBox, path/rect/polyline/line/circle/polygon).
// Stroke uses `color`; fill="none" icons are stroked only.

namespace gpui {

bool SvgDraw(PaintCtx* ctx, Str assetPath, float x, float y, float size,
             Rgba color);

// IconName -> "icons/<kebab>.svg" (same mapping as gpui-component's
// icon_named!).
Str IconNamePath(IconName name);
} // namespace gpui
