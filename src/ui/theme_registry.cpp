#include "ui/theme_registry.h"

#include "gpui/assets.h"

#include <math.h>
#include <stdio.h>

namespace gpui {

#if GPUI_OS_WINDOWS
static const char kSep = '\\';
#else
static const char kSep = '/';
#endif

// A whole text file, or an empty Str. Reading a file is plain stdio here;
// the asset loader's own reader answers for one relative path, and this walks
// a directory it has already resolved.
static Str ReadTextFile(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        return {};
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > (1 << 22)) {
        fclose(f);
        return {};
    }
    char* buf = AllocArray<char>((int)n + 1);
    if (!buf) {
        fclose(f);
        return {};
    }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = 0;
    return Str(buf, (int)got);
}

// ─── colour grammar — crates/ui/src/theme/color.rs ───────────────────────

static float Clamp01f(float v) {
    return v < 0 ? 0 : (v > 1 ? 1 : v);
}

static int HexDigit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

// gpui::Rgba::try_from(&str): three, four, six or eight hex digits after the
// hash, where the short forms double each digit.
static bool ParseHex(Str s, Rgba* out) {
    const char* p = s.s + 1;
    int n = s.len - 1;
    if (n != 3 && n != 4 && n != 6 && n != 8) {
        return false;
    }
    int d[8];
    for (int i = 0; i < n; i++) {
        d[i] = HexDigit(p[i]);
        if (d[i] < 0) {
            return false;
        }
    }
    int v[4] = {0, 0, 0, 255};
    if (n <= 4) {
        for (int i = 0; i < n; i++) {
            v[i] = d[i] * 16 + d[i];
        }
    } else {
        for (int i = 0; i < n / 2; i++) {
            v[i] = d[i * 2] * 16 + d[i * 2 + 1];
        }
    }
    *out = Rgba8((uint8_t)v[0], (uint8_t)v[1], (uint8_t)v[2], (uint8_t)v[3]);
    return true;
}

// ColorName::scale: the column for `scale`, or the one for 500 when no column
// carries that number.
static bool ScaleOf(const ShadcnScale* hue, int scale, Rgba* out) {
    int at = -1;
    int fallback = -1;
    for (int i = 0; i < kNumShadcnColumns; i++) {
        if (kShadcnScaleNums[i] == scale) {
            at = i;
        }
        if (kShadcnScaleNums[i] == 500) {
            fallback = i;
        }
    }
    if (at < 0) {
        at = fallback;
    }
    if (at < 0) {
        return false;
    }
    *out = RgbaHex(hue->hex[at]);
    return true;
}

static bool NameEq(const char* name, Str s) {
    int n = (int)strlen(name);
    if (n != s.len) {
        return false;
    }
    return StrCmpNI(name, s.s, n) == 0;
}

// The integer at [s, s+len), or -1 for anything that is not one. Rust uses
// `parse::<usize>()`, which is equally all-or-nothing.
static int ParseUint(const char* s, int len) {
    if (len <= 0 || len > 9) {
        return -1;
    }
    int v = 0;
    for (int i = 0; i < len; i++) {
        if (s[i] < '0' || s[i] > '9') {
            return -1;
        }
        v = v * 10 + (s[i] - '0');
    }
    return v;
}

static float ParseFloatOr(const char* s, int len, float fallback) {
    char buf[32];
    if (len <= 0 || len >= (int)sizeof(buf)) {
        return fallback;
    }
    memcpy(buf, s, (size_t)len);
    buf[len] = 0;
    char* end = nullptr;
    float v = (float)strtod(buf, &end);
    if (!end || *end) {
        return fallback;
    }
    return v;
}

bool ThemeParseColor(Str s, Rgba* out) {
    if (!s.s || s.len <= 0 || !out) {
        return false;
    }
    if (s.s[0] == '#') {
        return ParseHex(s, out);
    }
    // name[-scale][/opacity]. Rust splits on the first `-` and the first `/`,
    // so a name with neither is the whole string and takes scale 500.
    int dash = -1;
    int slash = -1;
    for (int i = 0; i < s.len; i++) {
        if (s.s[i] == '-' && dash < 0 && slash < 0) {
            dash = i;
        } else if (s.s[i] == '/' && slash < 0) {
            slash = i;
        }
    }
    int nameLen = dash >= 0 ? dash : (slash >= 0 ? slash : s.len);
    Str name = Str(s.s, nameLen);
    if (name.len <= 0) {
        return false;
    }
    int scale = 500;
    if (dash >= 0) {
        int end = slash >= 0 ? slash : s.len;
        scale = ParseUint(s.s + dash + 1, end - dash - 1);
        if (scale < 0) {
            // `parse::<usize>().ok()` leaving None is the same as no scale.
            scale = 500;
        }
    }
    Rgba c;
    if (NameEq("white", name)) {
        c = RgbaHex(kShadcnWhite);
    } else if (NameEq("black", name)) {
        c = RgbaHex(kShadcnBlack);
    } else {
        const ShadcnScale* hue = nullptr;
        for (int i = 0; i < kNumShadcnScales; i++) {
            if (NameEq(kShadcnScales[i].name, name)) {
                hue = &kShadcnScales[i];
                break;
            }
        }
        if (!hue || !ScaleOf(hue, scale, &c)) {
            return false;
        }
    }
    if (slash >= 0) {
        float pct = ParseFloatOr(s.s + slash + 1, s.len - slash - 1, -1.f);
        if (pct > 100.f) {
            return false;
        }
        if (pct >= 0) {
            c = RgbaOpacity(c, pct / 100.f);
        }
    }
    *out = c;
    return true;
}

// ─── the colour maths the fallbacks are written in ───────────────────────

// gpui::transparent_black(), which every `mix_oklab` toward nothing takes.
static Rgba Transparent() {
    return Rgba8(0, 0, 0, 0);
}

// gpui::Hsla::blend: `over` composited onto `base` by its own alpha. The
// result keeps the base's alpha, which is why `background.blend(x)` is opaque
// however faint `x` is.
static Rgba Blend(Rgba base, Rgba over) {
    if (over.a >= 255) {
        return over;
    }
    if (over.a == 0) {
        return base;
    }
    float f = over.a / 255.f;
    auto mix = [&](uint8_t b, uint8_t o) {
        return (uint8_t)(b * (1.f - f) + o * f + 0.5f);
    };
    return Rgba8(mix(base.r, over.r), mix(base.g, over.g), mix(base.b, over.b),
                 base.a);
}

// Colorize::lighten / ::darken, which scale the HSL lightness rather than
// mixing toward white or black.
static Rgba ScaleLightness(Rgba c, float factor) {
    float h = 0, s = 0, l = 0;
    RgbaToHsla(c, &h, &s, &l);
    return RgbaHsla(h, s, Clamp01f(l * factor), c.a / 255.f);
}

static Rgba Lighten(Rgba c, float amount) {
    return ScaleLightness(c, 1.f + Clamp01f(amount));
}

static Rgba Darken(Rgba c, float amount) {
    return ScaleLightness(c, 1.f - Clamp01f(amount));
}

static float ToLinear(float c) {
    return c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f);
}

static float FromLinear(float c) {
    return c <= 0.0031308f ? c * 12.92f : 1.055f * powf(c, 1.f / 2.4f) - 0.055f;
}

static void RgbToOklab(Rgba c, float* L, float* A, float* B) {
    float lr = ToLinear(c.r / 255.f);
    float lg = ToLinear(c.g / 255.f);
    float lb = ToLinear(c.b / 255.f);
    float l = 0.4122214708f * lr + 0.5363325363f * lg + 0.0514459929f * lb;
    float m = 0.2119034982f * lr + 0.6806995451f * lg + 0.1073969566f * lb;
    float s = 0.0883024619f * lr + 0.2817188376f * lg + 0.6299787005f * lb;
    float l_ = cbrtf(l), m_ = cbrtf(m), s_ = cbrtf(s);
    *L = 0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_;
    *A = 1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_;
    *B = 0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_;
}

static Rgba OklabToRgb(float L, float A, float B, float alpha) {
    float l_ = L + 0.3963377774f * A + 0.2158037573f * B;
    float m_ = L - 0.1055613458f * A - 0.0638541728f * B;
    float s_ = L - 0.0894841775f * A - 1.2914855480f * B;
    float l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;
    float lr = 4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
    float lg = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
    float lb = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;
    auto b8 = [](float v) {
        return (uint8_t)(Clamp01f(FromLinear(v)) * 255.f + 0.5f);
    };
    return Rgba8(b8(lr), b8(lg), b8(lb),
                 (uint8_t)(Clamp01f(alpha) * 255.f + 0.5f));
}

// Colorize::mix_oklab, which is CSS `color-mix(in oklab, a factor%, b)`: the
// alpha is interpolated first and the Oklab channels are premultiplied by it,
// so mixing toward transparent fades without dragging the hue to black.
static Rgba MixOklab(Rgba a, Rgba b, float factor) {
    factor = Clamp01f(factor);
    float inv = 1.f - factor;
    float aa = a.a / 255.f, ab = b.a / 255.f;
    float alpha = aa * factor + ab * inv;
    if (alpha <= 0) {
        return Transparent();
    }
    float l1, a1, b1, l2, a2, b2;
    RgbToOklab(a, &l1, &a1, &b1);
    RgbToOklab(b, &l2, &a2, &b2);
    float L = (l1 * aa * factor + l2 * ab * inv) / alpha;
    float A = (a1 * aa * factor + a2 * ab * inv) / alpha;
    float B = (b1 * aa * factor + b2 * ab * inv) / alpha;
    return OklabToRgb(L, A, B, alpha);
}

// ─── apply_config — crates/ui/src/theme/schema.rs ────────────────────────

// apply_color! / apply_background_color!: the config's value for `key` when
// it has one the grammar takes, and `fb` otherwise. The two macros differ in
// Rust only by whether a gradient is allowed through, and a `Theme` field
// here is one colour, so they are the same read.
// The keys default-theme.json spells differently from the serde names the
// schema declares, so serde drops the value and upstream paints the fallback
// instead: five chart blues collapse to one lightened ramp, the drag border
// goes from blue to the primary at 65%, and a description list's label loses
// its own colour. Every one of them is plainly what the theme's author meant,
// and a third-party file copying default-theme.json's spelling — which is the
// only spelling anyone reading that file would copy — would lose them the same
// way. So the second name is read too, after the schema's own.
static const char* const kKeyAliases[][2] = {
    {"chart.1", "chart_1"},
    {"chart.2", "chart_2"},
    {"chart.3", "chart_3"},
    {"chart.4", "chart_4"},
    {"chart.5", "chart_5"},
    {"drag.border", "drag_border"},
    {"progress.bar.background", "progress_bar.background"},
    {"description_list.label.background", "description_list_label.background"},
    {"description_list.label.foreground", "description_list_label.foreground"},
};

static Rgba Pick(const JsonValue* colors, const char* key, Rgba fb) {
    const JsonValue* v = JsonGet(colors, key);
    if (!v) {
        for (size_t i = 0; i < sizeof(kKeyAliases) / sizeof(kKeyAliases[0]);
             i++) {
            if (strcmp(kKeyAliases[i][0], key) == 0) {
                v = JsonGet(colors, kKeyAliases[i][1]);
                break;
            }
        }
    }
    if (!v || v->kind != JsonKind::String) {
        return fb;
    }
    Rgba c;
    if (!ThemeParseColor(v->str, &c)) {
        return fb;
    }
    return c;
}

// The last step of apply_config: a highlight that is allowed to cover a row
// of text can never be more opaque than this, however the file spells it.
static Rgba ClampAlpha(Rgba c, float max) {
    uint8_t cap = (uint8_t)(Clamp01f(max) * 255.f + 0.5f);
    if (c.a <= cap) {
        return c;
    }
    return Rgba8(c.r, c.g, c.b, cap);
}

void ThemeConfigResolve(Theme* out, const ThemeConfig* cfg, const Theme& base) {
    if (!out || !cfg) {
        return;
    }
    *out = base;
    const JsonValue* c = cfg->colors;
    bool dark = cfg->mode == ThemeMode::Dark;
    // The two constants every button and hover fallback is written against.
    const float activeDarken = dark ? 0.2f : 0.1f;
    const float hoverOpacity = 0.9f;
    const Rgba clear = Transparent();

    out->background = Pick(c, "background", base.background);

    // The base colours, which the semantic ones fall back to. Their `.light`
    // halves are the background blended with 80% of the base, and nothing in
    // this tree keeps them, so they stay local.
    out->red = Pick(c, "base.red", base.red);
    out->green = Pick(c, "base.green", base.green);
    out->blue = Pick(c, "base.blue", base.blue);
    out->magenta = Pick(c, "base.magenta", base.magenta);
    out->yellow = Pick(c, "base.yellow", base.yellow);
    out->cyan = Pick(c, "base.cyan", base.cyan);

    out->border = Pick(c, "border", base.border);
    out->foreground = Pick(c, "foreground", base.foreground);
    out->inputBorder = Pick(c, "input.border", out->border);
    out->muted = Pick(c, "muted.background", base.muted);
    out->mutedFg = Pick(c, "muted.foreground",
                        Blend(out->muted, RgbaOpacity(out->foreground, 0.7f)));

    // Theme::input_background(): a dark input sits on its own border mixed
    // toward transparent, a light one on the window background.
    out->inputBg =
        dark ? MixOklab(out->inputBorder, clear, 0.3f) : out->background;

    out->primary = Pick(c, "primary.background", base.primary);
    out->primaryFg = Pick(c, "primary.foreground", out->foreground);
    Rgba primaryHover =
        Pick(c, "primary.hover.background",
             Blend(out->background, RgbaOpacity(out->primary, hoverOpacity)));
    Rgba primaryActive = Pick(c, "primary.active.background",
                              Darken(out->primary, activeDarken));
    (void)primaryHover;
    (void)primaryActive;

    out->secondary = Pick(c, "secondary.background", base.secondary);
    out->secondaryFg = Pick(c, "secondary.foreground", out->foreground);
    out->secondaryHover =
        Pick(c, "secondary.hover.background",
             Blend(out->background, RgbaOpacity(out->secondary, hoverOpacity)));
    out->secondaryActive = Pick(c, "secondary.active.background",
                                Darken(out->secondary, activeDarken));

    out->success = Pick(c, "success.background", out->green);
    out->successFg = Pick(c, "success.foreground", out->primaryFg);
    out->info = Pick(c, "info.background", out->cyan);
    out->infoFg = Pick(c, "info.foreground", out->primaryFg);
    out->warning = Pick(c, "warning.background", out->yellow);
    out->warningFg = Pick(c, "warning.foreground", out->primaryFg);

    out->accent = Pick(c, "accent.background", out->secondary);
    Rgba accentFg = Pick(c, "accent.foreground", out->foreground);
    out->groupBox =
        Pick(c, "group_box.background",
             Blend(out->background,
                   RgbaOpacity(out->secondary, dark ? 0.3f : 0.4f)));
    out->groupBoxFg = Pick(c, "group_box.foreground", out->foreground);
    out->caret = Pick(c, "caret", out->primary);

    out->chart1 = Pick(c, "chart.1", Lighten(out->blue, 0.4f));
    out->chart2 = Pick(c, "chart.2", Lighten(out->blue, 0.2f));
    out->chart3 = Pick(c, "chart.3", out->blue);
    out->chart4 = Pick(c, "chart.4", Darken(out->blue, 0.2f));
    out->chart5 = Pick(c, "chart.5", Darken(out->blue, 0.4f));
    out->chartBullish = Pick(c, "chart_bullish", out->green);
    out->chartBearish = Pick(c, "chart_bearish", out->red);

    out->danger = Pick(c, "danger.background", out->red);
    out->dangerFg = Pick(c, "danger.foreground", out->primaryFg);
    out->descListLabel =
        Pick(c, "description_list.label.background",
             Blend(out->background, RgbaOpacity(out->border, 0.2f)));
    out->descListLabelFg =
        Pick(c, "description_list.label.foreground", out->mutedFg);
    out->dragBorder = Pick(c, "drag.border", RgbaOpacity(out->primary, 0.65f));

    Rgba list = Pick(c, "list.background", out->background);
    out->listActive =
        Pick(c, "list.active.background",
             Blend(out->background, RgbaOpacity(out->primary, 0.1f)));
    out->listActiveBorder =
        Pick(c, "list.active.border",
             Blend(out->background, RgbaOpacity(out->primary, 0.6f)));
    Rgba listEven = Pick(c, "list.even.background", list);
    Rgba listHead = Pick(c, "list.head.background", list);

    out->progress = Pick(c, "progress.bar.background", out->primary);
    out->ring = Pick(c, "ring", out->blue);
    out->scrollbarThumb = Pick(c, "scrollbar.thumb.background", out->accent);
    out->selection = Pick(c, "selection.background", out->primary);

    out->sidebar =
        Pick(c, "sidebar.background",
             Blend(out->background, RgbaOpacity(out->border, 0.15f)));
    out->sidebarAccent = Pick(c, "sidebar.accent.background", out->accent);
    out->sidebarAccentFg = Pick(c, "sidebar.accent.foreground", accentFg);
    out->sidebarBorder = Pick(c, "sidebar.border", out->border);
    out->sidebarFg = Pick(c, "sidebar.foreground", out->foreground);
    out->sidebarPrimary = Pick(c, "sidebar.primary.background", out->primary);
    out->sidebarPrimaryFg =
        Pick(c, "sidebar.primary.foreground", out->primaryFg);

    out->skeleton = Pick(c, "skeleton.background", out->secondary);
    out->tabActiveBg = Pick(c, "tab.active.background", out->background);
    out->tabActiveFg = Pick(c, "tab.active.foreground", out->foreground);
    out->tabBar = Pick(c, "tab_bar.background", out->background);
    out->tabFg = Pick(c, "tab.foreground", out->foreground);

    out->tableBg = Pick(c, "table.background", list);
    out->tableActive = Pick(c, "table.active.background", out->listActive);
    out->tableActiveBorder =
        Pick(c, "table.active.border", out->listActiveBorder);
    out->tableEven = Pick(c, "table.even.background", listEven);
    out->tableHead = Pick(c, "table.head.background", listHead);
    out->tableHeadFg = Pick(c, "table.head.foreground", out->mutedFg);
    out->tableRowBorder = Pick(c, "table.row.border", out->border);

    out->titleBar = Pick(c, "title_bar.background", out->background);
    out->titleBarBorder = Pick(c, "title_bar.border", out->border);
    out->overlay = Pick(c, "overlay", base.overlay);

    // The three that are painted over text, capped however the file spells
    // them: a row highlight at a fifth, a text selection at a third.
    out->listActive = ClampAlpha(out->listActive, 0.2f);
    out->tableActive = ClampAlpha(out->tableActive, 0.2f);
    out->selection = ClampAlpha(out->selection, 0.3f);

    // Theme::apply_config's metrics. `radius_lg` follows the radius the way
    // ThemeSetRadius does when the file names only the one.
    if (cfg->radius >= 0) {
        out->radius = cfg->radius;
        out->radiusLg = cfg->radius > 0 ? cfg->radius + 2 : 0;
    }
    if (cfg->radiusLg >= 0) {
        out->radiusLg = cfg->radiusLg;
    }
}

// ─── the registry — crates/ui/src/theme/registry.rs ──────────────────────

// The documents and the strings the configs point into. One arena for the
// lot, freed together, since a theme is never dropped on its own.
static Arena* gArena = nullptr;
static Vec<ThemeConfig> gThemes;
static bool gInited = false;
static Str gActive[2] = {};

static ThemeMode ParseMode(Str s) {
    return StrEqI(s, StrL("dark")) ? ThemeMode::Dark : ThemeMode::Light;
}

// sorted_themes: is_default first, then light before dark, then by name
// folded to lower case.
static bool SortsBefore(const ThemeConfig& a, const ThemeConfig& b) {
    if (a.isDefault != b.isDefault) {
        return a.isDefault;
    }
    if (a.mode != b.mode) {
        return a.mode == ThemeMode::Light;
    }
    return StrCmpI(a.name.s ? a.name.s : "", b.name.s ? b.name.s : "") < 0;
}

static void InsertSorted(const ThemeConfig& cfg) {
    int at = gThemes.len;
    for (int i = 0; i < gThemes.len; i++) {
        if (SortsBefore(cfg, gThemes[i])) {
            at = i;
            break;
        }
    }
    gThemes.InsertAt(at, cfg);
}

// A name is a `Str` into the arena, and `StrSame` wants a null terminator
// nowhere, so this is the plain comparison the table needs.
static bool SameName(Str a, Str b) {
    return a.len == b.len &&
           (a.len == 0 || memcmp(a.s, b.s, (size_t)a.len) == 0);
}

static float JsonFloatOr(const JsonValue* v, const char* key, float fallback) {
    const JsonValue* m = JsonGet(v, key);
    if (!m || m->kind != JsonKind::Number) {
        return fallback;
    }
    return (float)m->num;
}

int ThemeRegistryLoadStr(Str json) {
    ThemeRegistryInit();
    if (!gArena || !json.s || json.len <= 0) {
        return 0;
    }
    JsonValue* doc = JsonParse(gArena, json);
    if (!doc) {
        return 0;
    }
    const JsonValue* themes = JsonGet(doc, "themes");
    if (!themes || themes->kind != JsonKind::Array) {
        return 0;
    }
    Str setAuthor = JsonString(JsonGet(doc, "author"));
    Str setUrl = JsonString(JsonGet(doc, "url"));
    int added = 0;
    for (const JsonValue* t = themes->first; t; t = t->next) {
        if (t->kind != JsonKind::Object) {
            continue;
        }
        ThemeConfig cfg;
        cfg.name = JsonString(JsonGet(t, "name"));
        if (cfg.name.len <= 0 || ThemeRegistryFind(cfg.name)) {
            continue;
        }
        cfg.author = setAuthor;
        cfg.url = setUrl;
        cfg.mode = ParseMode(JsonString(JsonGet(t, "mode")));
        cfg.isDefault = JsonBool(JsonGet(t, "is_default"));
        cfg.colors = JsonGet(t, "colors");
        cfg.fontSize = JsonFloatOr(t, "font.size", 0);
        cfg.radius = JsonFloatOr(t, "radius", -1);
        cfg.radiusLg = JsonFloatOr(t, "radius.lg", -1);
        InsertSorted(cfg);
        added++;
    }
    return added;
}

void ThemeRegistryInit() {
    if (gInited) {
        return;
    }
    gInited = true;
    gArena = ArenaNew();
    AppOnShutdown(ThemeRegistryFree);
    // The two the tree was built with are what everything else resolves
    // against, so they go in first and stay first.
    ThemeRegistryLoadStr(Str(kDefaultThemeJson));
    for (int i = 0; i < gThemes.len; i++) {
        if (gThemes[i].isDefault) {
            gActive[(int)gThemes[i].mode] = gThemes[i].name;
        }
    }
}

int ThemeRegistryLoadDir(Str dir) {
    ThemeRegistryInit();
    char path[kMaxPath];
    int n = dir.len < kMaxPath - 1 ? dir.len : kMaxPath - 1;
    memcpy(path, dir.s ? dir.s : "", (size_t)n);
    path[n] = 0;
    char resolved[kMaxPath];
    if (!PlatDirExists(path)) {
        if (!AssetsFindDir(dir, resolved, kMaxPath)) {
            return 0;
        }
        StrCopyZ(path, kMaxPath, resolved);
    }
    // Rust reads the whole directory; a hundred entries is more themes than
    // anyone ships and the listing is a fixed buffer either way.
    const int kMaxEntries = 128;
    DirEntry* entries = AllocArray<DirEntry>(kMaxEntries);
    if (!entries) {
        return 0;
    }
    int count = PlatListDir(path, entries, kMaxEntries);
    int added = 0;
    for (int i = 0; i < count; i++) {
        if (entries[i].isDir) {
            continue;
        }
        const char* name = entries[i].name;
        int len = (int)strlen(name);
        if (len < 6 || StrCmpI(name + len - 5, ".json") != 0) {
            continue;
        }
        char file[kMaxPath];
        StrCopyZ(file, kMaxPath, path);
        int at = (int)strlen(file);
        if (at + 1 < kMaxPath) {
            file[at++] = kSep;
            StrCopyZ(file + at, kMaxPath - at, name);
        }
        Str text = ReadTextFile(file);
        if (text.s) {
            // An unparseable file is skipped rather than fatal, the way
            // Rust's `reload()` logs and carries on.
            added += ThemeRegistryLoadStr(text);
            StrFree(text);
        }
    }
    free(entries);
    return added;
}

int ThemeRegistryCount() {
    ThemeRegistryInit();
    return gThemes.len;
}

const ThemeConfig* ThemeRegistryAt(int ix) {
    ThemeRegistryInit();
    if (ix < 0 || ix >= gThemes.len) {
        return nullptr;
    }
    return &gThemes[ix];
}

const ThemeConfig* ThemeRegistryFind(Str name) {
    for (int i = 0; i < gThemes.len; i++) {
        if (SameName(gThemes[i].name, name)) {
            return &gThemes[i];
        }
    }
    return nullptr;
}

Str ThemeRegistryActive(ThemeMode mode) {
    ThemeRegistryInit();
    return gActive[(int)mode];
}

bool ThemeRegistryApply(App* app, const ThemeConfig* cfg) {
    ThemeRegistryInit();
    if (!cfg) {
        return false;
    }
    bool dark = cfg->mode == ThemeMode::Dark;
    Theme t;
    ThemeConfigResolve(&t, cfg,
                       dark ? ThemeDefaultDark() : ThemeDefaultLight());
    ThemeInstall(cfg->mode, t);
    gActive[(int)cfg->mode] = cfg->name;
    if (cfg->fontSize > 0) {
        ThemeSetFontSize(cfg->fontSize);
    }
    if (app) {
        AppRefreshWindows(app);
    }
    return true;
}

bool ThemeRegistryApply(App* app, Str name) {
    return ThemeRegistryApply(app, ThemeRegistryFind(name));
}

void ThemeRegistryReset(App* app) {
    ThemeRegistryInit();
    ThemeInstall(ThemeMode::Light, ThemeDefaultLight());
    ThemeInstall(ThemeMode::Dark, ThemeDefaultDark());
    for (int i = 0; i < gThemes.len; i++) {
        if (gThemes[i].isDefault) {
            gActive[(int)gThemes[i].mode] = gThemes[i].name;
        }
    }
    if (app) {
        AppRefreshWindows(app);
    }
}

void ThemeRegistryFree() {
    gThemes.Reset();
    if (gArena) {
        ArenaDelete(gArena);
        gArena = nullptr;
    }
    gActive[0] = {};
    gActive[1] = {};
    gInited = false;
}

} // namespace gpui
