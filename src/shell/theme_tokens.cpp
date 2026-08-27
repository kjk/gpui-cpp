#include "shell/theme_tokens.h"

#include "shell/scope.h"
#include "ui/theme.h"

namespace gpui::shell {

struct ShellThemeCache {
    ColorTokens colors = {};
    RadiusTokens radius = {};
    SpacingTokens spacing = {};
    bool valid = false;
};

static thread_local ShellThemeCache gThemeCache;

static void SyncCurrentScope() {
    ScopeHostContext host = ScopeCurrentHost();
    if (host.IsSet()) ThemeTokensSync(host.GetApp());
}

static const char kColorNames[] =
    "background\0foreground\0surface\0surface_foreground\0primary\0"
    "primary_foreground\0secondary\0secondary_foreground\0muted\0"
    "muted_foreground\0accent\0accent_foreground\0destructive\0"
    "destructive_foreground\0border\0input\0ring\0";
static const char kSpacingNames[] = "xxs\0xs\0sm\0md\0lg\0xl\0xxl\0";
static const char kRadiusNames[] = "none\0sm\0md\0lg\0xl\0full\0";

void ThemeTokensSync(const App* app) {
    if (!app) {
        return;
    }
    SemanticThemeTokens tokens = ThemeSemanticTokens(ThemeNow(app));
    gThemeCache.colors = tokens.colors;
    gThemeCache.radius = tokens.radius;
    gThemeCache.spacing = tokens.spacing;
    gThemeCache.valid = true;
}

static bool ColorOf(const ColorTokens& colors, Str name, Rgba* out) {
    if (StrEq(name, "background"))
        *out = colors.background;
    else if (StrEq(name, "foreground"))
        *out = colors.foreground;
    else if (StrEq(name, "surface"))
        *out = colors.surface;
    else if (StrEq(name, "surface_foreground"))
        *out = colors.surfaceForeground;
    else if (StrEq(name, "primary"))
        *out = colors.primary;
    else if (StrEq(name, "primary_foreground"))
        *out = colors.primaryForeground;
    else if (StrEq(name, "secondary"))
        *out = colors.secondary;
    else if (StrEq(name, "secondary_foreground"))
        *out = colors.secondaryForeground;
    else if (StrEq(name, "muted"))
        *out = colors.muted;
    else if (StrEq(name, "muted_foreground"))
        *out = colors.mutedForeground;
    else if (StrEq(name, "accent"))
        *out = colors.accent;
    else if (StrEq(name, "accent_foreground"))
        *out = colors.accentForeground;
    else if (StrEq(name, "destructive"))
        *out = colors.destructive;
    else if (StrEq(name, "destructive_foreground"))
        *out = colors.destructiveForeground;
    else if (StrEq(name, "border"))
        *out = colors.border;
    else if (StrEq(name, "input"))
        *out = colors.input;
    else if (StrEq(name, "ring"))
        *out = colors.ring;
    else
        return false;
    return true;
}

bool ThemeTokenColor(Str name, Hsla* out) {
    SyncCurrentScope();
    if (!out || !gThemeCache.valid) {
        return false;
    }
    Rgba value = {};
    if (!ColorOf(gThemeCache.colors, name, &value)) {
        return false;
    }
    *out = HslaFromRgba(value);
    return true;
}

bool ThemeTokenSpacing(Str name, float* out) {
    SyncCurrentScope();
    if (!out || !gThemeCache.valid) {
        return false;
    }
    if (StrEq(name, "xxs"))
        *out = gThemeCache.spacing.xxs;
    else if (StrEq(name, "xs"))
        *out = gThemeCache.spacing.xs;
    else if (StrEq(name, "sm"))
        *out = gThemeCache.spacing.sm;
    else if (StrEq(name, "md"))
        *out = gThemeCache.spacing.md;
    else if (StrEq(name, "lg"))
        *out = gThemeCache.spacing.lg;
    else if (StrEq(name, "xl"))
        *out = gThemeCache.spacing.xl;
    else if (StrEq(name, "xxl"))
        *out = gThemeCache.spacing.xxl;
    else
        return false;
    return true;
}

bool ThemeTokenRadius(Str name, float* out) {
    SyncCurrentScope();
    if (!out || !gThemeCache.valid) {
        return false;
    }
    if (StrEq(name, "none"))
        *out = gThemeCache.radius.none;
    else if (StrEq(name, "sm"))
        *out = gThemeCache.radius.sm;
    else if (StrEq(name, "md"))
        *out = gThemeCache.radius.md;
    else if (StrEq(name, "lg"))
        *out = gThemeCache.radius.lg;
    else if (StrEq(name, "xl"))
        *out = gThemeCache.radius.xl;
    else if (StrEq(name, "full"))
        *out = gThemeCache.radius.full;
    else
        return false;
    return true;
}

SeqStrings ThemeColorTokenNames() {
    return kColorNames;
}
SeqStrings ThemeSpacingTokenNames() {
    return kSpacingNames;
}
SeqStrings ThemeRadiusTokenNames() {
    return kRadiusNames;
}

} // namespace gpui::shell
