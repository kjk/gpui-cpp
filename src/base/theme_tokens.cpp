#include "base/theme_tokens.h"

namespace gpui {

SemanticShadowTokens SemanticShadowElevations(Rgba color) {
    SemanticShadowTokens out;
    out.has = true;
    out.sm = {0, 1, 2, 0, color};
    out.md = {0, 4, 8, -2, color};
    out.lg = {0, 12, 24, -4, color};
    return out;
}

} // namespace gpui
