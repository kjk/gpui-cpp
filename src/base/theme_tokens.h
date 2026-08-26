/* Semantic design tokens — crates/base/src/theme_tokens.rs
 *
 * The parallel token layer upstream is migrating to: visual *roles* and
 * scales, named without a component in sight — no `button`, no `table`,
 * no `sidebar`. Rust keeps them on `gpui_base::Theme` and the `crates/ui`
 * theme projects them out of its own palette every time it is applied;
 * the same pair of functions is below.
 *
 * `ThemeTokens` in `gpui/gpui.h` is a different set with a confusingly
 * similar name: it is the ui layer's own, and it carries gradients. */

#include "gpui/gpui.h"

namespace gpui {

struct SemanticColorTokens {
    Rgba background = {};
    Rgba foreground = {};
    Rgba surface = {};
    Rgba surfaceForeground = {};
    Rgba primary = {};
    Rgba primaryForeground = {};
    Rgba secondary = {};
    Rgba secondaryForeground = {};
    Rgba muted = {};
    Rgba mutedForeground = {};
    Rgba accent = {};
    Rgba accentForeground = {};
    Rgba destructive = {};
    Rgba destructiveForeground = {};
    Rgba border = {};
    Rgba input = {};
    Rgba ring = {};
};

struct SemanticRadiusTokens {
    float none = 0;
    float sm = 3;
    float md = 6;
    float lg = 8;
    float xl = 12;
    float full = 9999;
};

struct SemanticSpacingTokens {
    float xxs = 2;
    float xs = 4;
    float sm = 8;
    float md = 12;
    float lg = 16;
    float xl = 24;
    float xxl = 32;
};

// One text role: a size, the line box it sits in, and a weight. Rust's
// FontWeight is a float where 400 is normal.
struct SemanticTextStyle {
    float size = 16;
    float lineHeight = 24;
    float weight = 400;
};

struct SemanticTypographyTokens {
    Str sans = {};
    Str mono = {};
    SemanticTextStyle xs = {12, 16, 400};
    SemanticTextStyle sm = {14, 20, 400};
    SemanticTextStyle md = {16, 24, 400};
    SemanticTextStyle lg = {18, 28, 400};
    SemanticTextStyle xl = {20, 28, 400};
    SemanticTextStyle monoMd = {13, 20, 400};
};

// One elevation: an offset, a blur, a spread and a colour. Rust's BoxShadow
// carries an `inset` flag too; nothing in the token set sets it.
struct SemanticShadow {
    float x = 0;
    float y = 0;
    float blur = 0;
    float spread = 0;
    Rgba color = {};
};

struct SemanticShadowTokens {
    // Whether each elevation is defined at all — Rust holds a Vec per level
    // and an empty one means no shadow, which is what `Theme::shadow` reads.
    bool has = false;
    SemanticShadow sm = {};
    SemanticShadow md = {};
    SemanticShadow lg = {};
};

// theme.mono_font_size: 13, which is what a code editor's rows are drawn at.
const float kMonoFontSize = 13.f;

// ShadowTokens::elevations: the three levels, from one colour.
SemanticShadowTokens SemanticShadowElevations(Rgba color);

struct SemanticThemeTokens {
    SemanticColorTokens colors = {};
    SemanticRadiusTokens radius = {};
    SemanticSpacingTokens spacing = {};
    SemanticTypographyTokens typography = {};
    SemanticShadowTokens shadow = {};
};

// Theme::semantic_tokens: the roles this palette comes to. `sm` is half the
// radius and `xl` twice it, which is what `radius_tokens` says.
SemanticThemeTokens ThemeSemanticTokens(const Theme& t,
                                        float fontSize = 16.f);
// Theme::apply_semantic_tokens: the other way, for a theme written as tokens
// rather than as the legacy key list. Only what the legacy palette can hold
// comes back — the spacing scale and the text roles have nowhere to go.
void ThemeApplySemanticTokens(Theme* t, const SemanticThemeTokens& tokens);

} // namespace gpui
