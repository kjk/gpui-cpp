#include "base/theme_tokens.h"

namespace gpui {

ShadowTokens ShadowTokens::Elevations(Rgba color) {
    ShadowTokens out;
    VecAppend(out.sm, BoxShadow{0, 1, 2, 0, color, false});
    VecAppend(out.md, BoxShadow{0, 4, 8, -2, color, false});
    VecAppend(out.lg, BoxShadow{0, 12, 24, -4, color, false});
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
