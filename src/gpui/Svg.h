#pragma once

#include "gpui/Gpui.h"

// Lucide-style SVG (viewBox, path/rect/polyline/line/circle/polygon).
// Stroke uses `color`; fill="none" icons are stroked only.
bool SvgDraw(PaintCtx* ctx, Str assetPath, float x, float y, float size, Rgba color);

// IconName -> "icons/<kebab>.svg" (same mapping as gpui-component's icon_named!).
Str IconNamePath(IconName name);
