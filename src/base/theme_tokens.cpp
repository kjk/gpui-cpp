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

SemanticThemeTokens ThemeSemanticTokens(const Theme& t) {
    SemanticThemeTokens out;
    SemanticColorTokens& c = out.colors;
    c.background = t.background;
    c.foreground = t.foreground;
    // `surface` is the popover pair: the role a thing floating over the page
    // plays, which is the closest the legacy palette has to a raised surface.
    c.surface = t.popover;
    c.surfaceForeground = t.popoverFg;
    c.primary = t.primary;
    c.primaryForeground = t.primaryFg;
    c.secondary = t.secondary;
    c.secondaryForeground = t.secondaryFg;
    c.muted = t.muted;
    c.mutedForeground = t.mutedFg;
    c.accent = t.accent;
    c.accentForeground = t.accentFg;
    c.destructive = t.danger;
    c.destructiveForeground = t.dangerFg;
    c.border = t.border;
    c.input = t.inputBorder;
    c.ring = t.ring;

    out.radius.none = 0;
    out.radius.sm = t.radius / 2.f;
    out.radius.md = t.radius;
    out.radius.lg = t.radiusLg;
    out.radius.xl = t.radius * 2.f;
    out.radius.full = 9999.f;

    // `typography_tokens` overwrites two sizes and the two families and
    // leaves the rest of the scale at its defaults. The families are empty
    // here: this tree names no font on the theme — the paint layer asks the
    // platform for the UI face and for its monospace one.
    out.typography.md.size = ThemeFontSize();
    out.typography.monoMd.size = kMonoFontSize;

    // `shadow_tokens`: the three elevations at 18% black. Rust gates them on
    // `Theme::shadow`, a flag a theme file can clear; nothing here reads such
    // a flag, so the elevations are always the ones a shadow would use.
    out.shadow = SemanticShadowElevations(Rgba8(0, 0, 0, 46));
    return out;
}

void ThemeApplySemanticTokens(Theme* t, const SemanticThemeTokens& tokens) {
    if (!t) {
        return;
    }
    const SemanticColorTokens& c = tokens.colors;
    t->background = c.background;
    t->foreground = c.foreground;
    t->popover = c.surface;
    t->popoverFg = c.surfaceForeground;
    t->primary = c.primary;
    t->primaryFg = c.primaryForeground;
    t->secondary = c.secondary;
    t->secondaryFg = c.secondaryForeground;
    t->muted = c.muted;
    t->mutedFg = c.mutedForeground;
    t->accent = c.accent;
    t->accentFg = c.accentForeground;
    t->danger = c.destructive;
    t->dangerFg = c.destructiveForeground;
    t->border = c.border;
    t->inputBorder = c.input;
    t->ring = c.ring;
    // The seven tokens Rust writes back beside the flat colours, so a
    // gradient left over from the palette this was applied to does not
    // outlive the colour under it.
    t->tokens.background = Background(c.background);
    t->tokens.popover = Background(c.surface);
    t->tokens.primary = Background(c.primary);
    t->tokens.secondary = Background(c.secondary);
    t->tokens.muted = Background(c.muted);
    t->tokens.accent = Background(c.accent);
    t->tokens.danger = Background(c.destructive);
    t->radius = tokens.radius.md;
    t->radiusLg = tokens.radius.lg;
    // What has nowhere to go, and is the reason this is a subset: the spacing
    // scale, the text roles, and the two font families.
}

} // namespace gpui
