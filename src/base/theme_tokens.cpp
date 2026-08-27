#include "base/theme_tokens.h"

namespace gpui {

ShadowTokens ShadowTokens::Elevations(Rgba color) {
    ShadowTokens out;
    out.sm.Append(BoxShadow{0, 1, 2, 0, color, false});
    out.md.Append(BoxShadow{0, 4, 8, -2, color, false});
    out.lg.Append(BoxShadow{0, 12, 24, -4, color, false});
    return out;
}

SemanticShadowTokens SemanticShadowElevations(Rgba color) {
    return ShadowTokens::Elevations(color);
}

const BoxShadow* ShadowFirst(const Vec<BoxShadow>& level) {
    return level.len > 0 ? &level[0] : nullptr;
}

BoxShadow* ShadowFirst(Vec<BoxShadow>& level) {
    return level.len > 0 ? &level[0] : nullptr;
}

} // namespace gpui
