/* The theme registry, against crates/ui/src/theme/{color,schema,registry}.rs */

#include "Test.h"

#include <stdio.h>

static Rgba Parsed(const char* s) {
    Rgba c = {};
    utassert(ThemeParseColor(Str(s), &c));
    return c;
}

static bool Is(Rgba c, uint32_t rgb, uint8_t a = 255) {
    return c.r == ((rgb >> 16) & 0xff) && c.g == ((rgb >> 8) & 0xff) &&
           c.b == (rgb & 0xff) && c.a == a;
}

// try_parse_color: the whole grammar a theme file writes a colour in.
static void AColourIsAHexOrAName() {
    utassert(Is(Parsed("#ff8800"), 0xff8800));
    utassert(Is(Parsed("#f80"), 0xff8800));
    // The short forms double each digit, so #f80c is the same as #ff8800cc.
    utassert(Is(Parsed("#f80c"), 0xff8800, 0xcc));
    utassert(Is(Parsed("#ff880080"), 0xff8800, 0x80));
    utassert(Is(Parsed("white"), 0xffffff));
    utassert(Is(Parsed("black"), 0x000000));
    // A name with no scale is the 500 column.
    utassert(Is(Parsed("neutral-500"), 0x737373));
    utassert(Is(Parsed("neutral"), 0x737373));
    utassert(Is(Parsed("neutral-200"), 0xe5e5e5));
    // And a scale no hue carries falls back to 500 rather than failing.
    utassert(Is(Parsed("neutral-123"), 0x737373));
    // `/percent` is the alpha, out of a hundred.
    utassert(Is(Parsed("red-500/50"), 0xef4444, 128));

    Rgba c = {};
    utassert(!ThemeParseColor(StrL("nosuchcolour-500"), &c));
    utassert(!ThemeParseColor(StrL("#12345"), &c));
    utassert(!ThemeParseColor(StrL(""), &c));
    // An opacity over a hundred is an error, not a clamp.
    utassert(!ThemeParseColor(StrL("red-500/140"), &c));
}

// The embedded default-theme.json is what the registry starts with, and its
// two entries are the pair every other theme is resolved against.
static void TheDefaultsAreInTheTable() {
    utassert(ThemeRegistryCount() >= 2);
    const ThemeConfig* light = ThemeRegistryFind(StrL("Default Light"));
    const ThemeConfig* dark = ThemeRegistryFind(StrL("Default Dark"));
    utassert(light && dark);
    utassert(light->isDefault && dark->isDefault);
    utassert(light->mode == ThemeMode::Light);
    utassert(dark->mode == ThemeMode::Dark);
    utassert(light->colors && dark->colors);
    // sorted_themes puts the defaults first and light before dark.
    utassert(ThemeRegistryAt(0) == light);
    utassert(ThemeRegistryAt(1) == dark);
    utassert(ThemeRegistryFind(StrL("No Such Theme")) == nullptr);
}

// Resolving the file the hardcoded palette was transcribed from has to give
// the hardcoded palette back. That is the whole point of reading the file:
// the two cannot drift without this failing.
static void TheDefaultThemeResolvesToTheDefaultPalette() {
    struct Token {
        const char* name;
        size_t off;
    };
#define TOK(f) {#f, offsetof(Theme, f)}
    static const Token kTokens[] = {
        TOK(background),      TOK(foreground),      TOK(border),
        TOK(mutedFg),         TOK(inputBorder),     TOK(inputBg),
        TOK(ring),            TOK(caret),           TOK(selection),
        TOK(dragBorder),      TOK(chart1),          TOK(chart2),
        TOK(chart3),          TOK(chart4),          TOK(chart5),
        TOK(chartBullish),    TOK(chartBearish),
        TOK(titleBar),        TOK(titleBarBorder),  TOK(tabBar),
        TOK(tabActiveBg),     TOK(tabActiveFg),     TOK(tabFg),
        TOK(tableBg),         TOK(tableHead),       TOK(tableHeadFg),
        TOK(tableRowBorder),  TOK(tableEven),       TOK(listActive),
        TOK(listActiveBorder), TOK(tableActive),    TOK(tableActiveBorder),
        TOK(progress),        TOK(red),             TOK(green),
        TOK(blue),            TOK(yellow),          TOK(cyan),
        TOK(magenta),         TOK(danger),          TOK(dangerFg),
        TOK(secondaryHover),  TOK(secondaryActive), TOK(secondaryFg),
        TOK(secondary),       TOK(muted),           TOK(accent),
        TOK(primary),         TOK(primaryFg),       TOK(sidebar),
        TOK(sidebarFg),       TOK(sidebarPrimary),  TOK(sidebarPrimaryFg),
        TOK(sidebarAccent),   TOK(sidebarAccentFg), TOK(sidebarBorder),
        TOK(scrollbarThumb),  TOK(info),            TOK(infoFg),
        TOK(success),         TOK(successFg),       TOK(warning),
        TOK(warningFg),       TOK(skeleton),        TOK(overlay),
        TOK(groupBox),        TOK(groupBoxFg),      TOK(descListLabel),
        TOK(descListLabelFg),
    };
#undef TOK

    const char* names[2] = {"Default Light", "Default Dark"};
    const Theme* bases[2] = {&ThemeDefaultLight(), &ThemeDefaultDark()};
    for (int m = 0; m < 2; m++) {
        const ThemeConfig* cfg = ThemeRegistryFind(Str(names[m]));
        utassert(cfg != nullptr);
        if (!cfg) {
            continue;
        }
        Theme got = {};
        ThemeConfigResolve(&got, cfg, *bases[m]);
        for (size_t i = 0; i < sizeof(kTokens) / sizeof(kTokens[0]); i++) {
            Rgba a = *(const Rgba*)((const char*)&got + kTokens[i].off);
            Rgba b = *(const Rgba*)((const char*)bases[m] + kTokens[i].off);
            bool same = a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
            if (!same) {
                printf("  theme drift %s.%s: file %02x%02x%02x%02x, "
                       "hardcoded %02x%02x%02x%02x\n",
                       names[m], kTokens[i].name, a.r, a.g, a.b, a.a, b.r, b.g,
                       b.b, b.a);
            }
            utassert(same);
        }
    }
}

void TestThemeRegistry() {
    TestSuite("theme_registry");
    AColourIsAHexOrAName();
    TheDefaultsAreInTheTable();
    TheDefaultThemeResolvesToTheDefaultPalette();
}
