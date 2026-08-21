/* The palette, against crates/ui/src/theme/default-theme.json resolved through
 * default-colors.json.
 *
 * Only a handful of the sixty tokens are pinned here, and they are the ones a
 * reading of the two files by hand got wrong: a semantic foreground in a dark
 * theme is the saturated colour rather than the near-black the light theme
 * puts on the same surface, which is easy to assume the other way round. The
 * anchors either side of them are there so a wholesale palette change cannot
 * pass. */

#include "Test.h"

static bool IsA(Rgba c, uint32_t hex, uint8_t a) {
    return c.r == ((hex >> 16) & 0xff) && c.g == ((hex >> 8) & 0xff) &&
           c.b == (hex & 0xff) && c.a == a;
}

static bool Is(Rgba c, uint32_t hex) {
    return IsA(c, hex, 255);
}

static void TheLightPaletteIsDefaultLight() {
    const Theme& t = ThemeLight();
    utassert(Is(t.background, 0xffffff));
    utassert(Is(t.foreground, 0x0a0a0a));
    utassert(Is(t.border, 0xe5e5e5));
    utassert(Is(t.mutedFg, 0x737373));
    utassert(Is(t.primary, 0x171717));
    // The four semantic pairs. A light theme's is the saturated background
    // with a near-white foreground on it.
    utassert(Is(t.danger, 0xef4444) && Is(t.dangerFg, 0xfafafa));
    utassert(Is(t.info, 0x06b6d4) && Is(t.infoFg, 0xfafafa));
    utassert(Is(t.success, 0x22c55e) && Is(t.successFg, 0xfafafa));
    utassert(Is(t.warning, 0xeab308) && Is(t.warningFg, 0xfafafa));
    utassert(Is(t.skeleton, 0xf5f5f5));
    // selection.background is a colour of its own, not `accent` faded — which
    // on a white field is a tint nobody can see. apply_config caps whatever a
    // file writes there at 30%, since it is painted over a row of text.
    utassert(IsA(t.selection, 0x55a0fc, 0x4d));
}

static void TheDarkPaletteIsDefaultDark() {
    const Theme& t = ThemeDark();
    utassert(Is(t.background, 0x0a0a0a));
    utassert(Is(t.foreground, 0xfafafa));
    utassert(Is(t.border, 0x262626));
    utassert(Is(t.primary, 0xfafafa));
    // And a dark theme's foreground is the 600 of the same hue, not the
    // near-black the light theme would put there.
    utassert(Is(t.danger, 0xf87171) && Is(t.dangerFg, 0xdc2626));
    utassert(Is(t.info, 0x22d3ee) && Is(t.infoFg, 0x0891b2));
    utassert(Is(t.success, 0x4ade80) && Is(t.successFg, 0x16a34a));
    utassert(Is(t.warning, 0xfacc15) && Is(t.warningFg, 0xca8a04));
    utassert(Is(t.skeleton, 0x171717));
    utassert(IsA(t.selection, 0x1d4ed8, 0x4d));
}

void TestThemeColor() {
    TestSuite("theme_color");
    TheLightPaletteIsDefaultLight();
    TheDarkPaletteIsDefaultDark();
}
