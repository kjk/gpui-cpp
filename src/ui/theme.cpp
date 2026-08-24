#include "ui/theme.h"

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

// ─── the gradient grammar — color.rs parse_linear_gradient ───────────────

static Str TrimStr(Str s) {
    while (s.len > 0 && (s.s[0] == ' ' || s.s[0] == '\t' || s.s[0] == '\n' ||
                         s.s[0] == '\r')) {
        s.s++;
        s.len--;
    }
    while (s.len > 0) {
        char c = s.s[s.len - 1];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            break;
        }
        s.len--;
    }
    return s;
}

static bool EqIgnoreCase(Str s, const char* lit) {
    int n = 0;
    while (lit[n]) {
        n++;
    }
    if (s.len != n) {
        return false;
    }
    for (int i = 0; i < n; i++) {
        char c = s.s[i];
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        if (c != lit[i]) {
            return false;
        }
    }
    return true;
}

static bool StartsWithIgnoreCase(Str s, const char* lit) {
    int n = 0;
    while (lit[n]) {
        n++;
    }
    if (s.len < n) {
        return false;
    }
    return EqIgnoreCase(Str(s.s, n), lit);
}

// split_top_level_commas: a comma inside parentheses belongs to whatever is
// there — `rgb(1, 2, 3)` is one stop, not three.
static int SplitTopLevelCommas(Str inner, Str* out, int cap) {
    int n = 0;
    int depth = 0;
    int start = 0;
    for (int i = 0; i < inner.len; i++) {
        char c = inner.s[i];
        if (c == '(') {
            depth++;
        } else if (c == ')') {
            if (depth > 0) {
                depth--;
            }
        } else if (c == ',' && depth == 0) {
            if (n < cap) {
                out[n] = TrimStr(Str(inner.s + start, i - start));
            }
            n++;
            start = i + 1;
        }
    }
    if (n < cap) {
        out[n] = TrimStr(Str(inner.s + start, inner.len - start));
    }
    n++;
    return n;
}

// parse_linear_gradient_direction: the eight `to ..` keywords, as degrees.
static bool ParseGradientDirection(Str dir, float* out) {
    bool top = false, right = false, bottom = false, left = false;
    int i = 0;
    while (i < dir.len) {
        while (i < dir.len && (dir.s[i] == ' ' || dir.s[i] == '\t')) {
            i++;
        }
        int start = i;
        while (i < dir.len && dir.s[i] != ' ' && dir.s[i] != '\t') {
            i++;
        }
        if (i == start) {
            break;
        }
        Str word = Str(dir.s + start, i - start);
        if (EqIgnoreCase(word, "top")) {
            top = true;
        } else if (EqIgnoreCase(word, "right")) {
            right = true;
        } else if (EqIgnoreCase(word, "bottom")) {
            bottom = true;
        } else if (EqIgnoreCase(word, "left")) {
            left = true;
        } else {
            return false;
        }
    }
    if (top && !right && !bottom && !left) {
        *out = 0.f;
    } else if (!top && right && !bottom && !left) {
        *out = 90.f;
    } else if (!top && !right && bottom && !left) {
        *out = 180.f;
    } else if (!top && !right && !bottom && left) {
        *out = 270.f;
    } else if (top && right && !bottom && !left) {
        *out = 45.f;
    } else if (!top && right && bottom && !left) {
        *out = 135.f;
    } else if (!top && !right && bottom && left) {
        *out = 225.f;
    } else if (top && !right && !bottom && left) {
        *out = 315.f;
    } else {
        return false;
    }
    return true;
}

// parse_linear_gradient_angle: `135deg`, or `to bottom right`.
static bool ParseGradientAngle(Str angle, float* out) {
    angle = TrimStr(angle);
    if (angle.len > 3 && EqIgnoreCase(Str(angle.s + angle.len - 3, 3), "deg")) {
        Str num = TrimStr(Str(angle.s, angle.len - 3));
        float deg = ParseFloatOr(num.s, num.len, 1e30f);
        if (deg >= 1e29f) {
            return false;
        }
        // rem_euclid(360): a negative angle comes back positive.
        deg = fmodf(deg, 360.f);
        if (deg < 0) {
            deg += 360.f;
        }
        *out = deg;
        return true;
    }
    if (StartsWithIgnoreCase(angle, "to ")) {
        return ParseGradientDirection(TrimStr(Str(angle.s + 3, angle.len - 3)),
                                      out);
    }
    return false;
}

// parse_linear_color_stop: a colour, and optionally where along the line it
// sits — `red-500 25%`. Without one it takes the end it was given.
static bool ParseColorStop(Str stop, float defaultPct, ColorStop* out) {
    stop = TrimStr(stop);
    if (stop.len <= 0) {
        return false;
    }
    float pct = defaultPct;
    if (stop.s[stop.len - 1] == '%') {
        // The percentage is the last whitespace-separated word.
        int i = stop.len - 1;
        while (i > 0 && stop.s[i - 1] != ' ' && stop.s[i - 1] != '\t') {
            i--;
        }
        float v = ParseFloatOr(stop.s + i, stop.len - 1 - i, 1e30f);
        if (v >= 1e29f) {
            return false;
        }
        pct = Clamp01f(v / 100.f);
        stop = TrimStr(Str(stop.s, i));
        if (stop.len <= 0) {
            return false;
        }
    }
    Rgba c;
    if (!ThemeParseColor(stop, &c)) {
        return false;
    }
    out->color = c;
    out->percentage = pct;
    return true;
}

static bool ParseLinearGradient(Str s, Background* out) {
    s = TrimStr(s);
    if (!StartsWithIgnoreCase(s, "linear-gradient(") || s.s[s.len - 1] != ')') {
        return false;
    }
    const int kPrefix = 16; // "linear-gradient("
    Str inner = Str(s.s + kPrefix, s.len - kPrefix - 1);
    Str parts[4] = {};
    int n = SplitTopLevelCommas(inner, parts, 4);
    float angle = 180.f;
    Str fromS = {}, toS = {};
    if (n == 2) {
        fromS = parts[0];
        toS = parts[1];
    } else if (n == 3) {
        if (!ParseGradientAngle(parts[0], &angle)) {
            return false;
        }
        fromS = parts[1];
        toS = parts[2];
    } else {
        // Rust takes exactly two stops; anything else is an error, which
        // leaves the token on its fallback.
        return false;
    }
    ColorStop from = {}, to = {};
    if (!ParseColorStop(fromS, 0.f, &from) || !ParseColorStop(toS, 1.f, &to)) {
        return false;
    }
    *out = BackgroundLinear(angle, from, to);
    return true;
}

bool ThemeParseBackground(Str s, Background* out) {
    if (!s.s || s.len <= 0 || !out) {
        return false;
    }
    Rgba c;
    if (ThemeParseColor(s, &c)) {
        *out = Background(c);
        return true;
    }
    return ParseLinearGradient(s, out);
}

// ─── the colour maths the fallbacks are written in ───────────────────────

// The colour maths lives in gpui.cpp now — a palette written in code derives
// its own tokens with it — and these are the names schema.rs's fallbacks are
// written in here.
static Rgba Transparent() {
    return RgbaTransparent();
}
static Rgba Blend(Rgba base, Rgba over) {
    return RgbaBlend(base, over);
}
static Rgba Lighten(Rgba c, float amount) {
    return RgbaLighten(c, amount);
}
static Rgba Darken(Rgba c, float amount) {
    return RgbaDarken(c, amount);
}
static Rgba MixOklab(Rgba a, Rgba b, float factor) {
    return RgbaMixOklab(a, b, factor);
}

// ─── apply_config — crates/ui/src/theme/schema.rs ────────────────────────

// apply_color! / apply_background_color!: the config's value for `key` when
// it has one the grammar takes, and `fb` otherwise. The two macros differ in
// Rust only by whether a gradient is allowed through, and a `Theme` field
// here is one colour, so they are the same read.
// The keys default-theme.json spells differently from the serde names the
// schema declares, so serde drops the value and upstream paints the fallback
// instead: five chart blues collapse to one lightened ramp, the drag border
// goes from blue to the primary at 65%. Every one of them is plainly what the
// theme's author meant, and a third-party file copying default-theme.json's
// spelling — which is the only spelling anyone reading that file would copy —
// would lose them the same way. So the second name is read too, after the
// schema's own.
//
// `description_list_label.background` and `.foreground` were on this list and
// are not any more. They are the two where reading the file's spelling makes
// a *visible* difference from the app this tree is a port of: a label painted
// in the foreground rather than in muted_foreground, which is what the Rust
// gallery shows on every row of its DescriptionList page. Matching the
// reference wins over honouring a key it ignores.
static const char* const kKeyAliases[][2] = {
    {"chart.1", "chart_1"},
    {"chart.2", "chart_2"},
    {"chart.3", "chart_3"},
    {"chart.4", "chart_4"},
    {"chart.5", "chart_5"},
    {"drag.border", "drag_border"},
    {"progress.bar.background", "progress_bar.background"},
};

// The string a token's key names, under the schema's spelling or the one
// default-theme.json uses. Null for a key the file leaves out, and for a
// value that is not a string.
static const JsonValue* FindColor(const JsonValue* colors, const char* key) {
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
    return (v && v->kind == JsonKind::String) ? v : nullptr;
}

static Rgba Pick(const JsonValue* colors, const char* key, Rgba fb) {
    const JsonValue* v = FindColor(colors, key);
    if (!v) {
        return fb;
    }
    Rgba c;
    if (!ThemeParseColor(v->str, &c)) {
        return fb;
    }
    return c;
}

// apply_background_color!: the same read, with a gradient allowed through.
static Background PickBg(const JsonValue* colors, const char* key,
                         Background fb) {
    const JsonValue* v = FindColor(colors, key);
    if (!v) {
        return fb;
    }
    Background b;
    if (!ThemeParseBackground(v->str, &b)) {
        return fb;
    }
    return b;
}

// The token takes the fill; the flat field beside it takes the token's
// colour, which for a gradient is its first stop.
static void SetToken(Rgba* flat, Background* tok, Background b) {
    *flat = b.color;
    *tok = b;
}

// The last step of apply_config: a highlight that is allowed to cover a row
// of text can never be more opaque than this, however the file spells it.
static Rgba ClampAlpha(Rgba c, float max) {
    // Truncated, like every other float→byte in the palette.
    uint8_t cap = (uint8_t)(Clamp01f(max) * 255.f);
    if (c.a <= cap) {
        return c;
    }
    return Rgba8(c.r, c.g, c.b, cap);
}

// The same cap over a token. A value the file named has each of its stops
// capped on its own, so a bright second stop cannot push the highlight past
// the cap; one that came from a fallback is scaled as a whole, which is what
// `Background::opacity` does and what Rust's `clamp_alpha` falls back to.
static void ClampToken(Rgba* flat, Background* tok, bool raw, float max) {
    float a = tok->color.a / 255.f;
    float target = a < max ? a : max;
    Background b = raw ? BackgroundClampAlpha(*tok, max)
                       : BackgroundOpacity(*tok, a > 0 ? target / a : 1.f);
    b.color = ClampAlpha(tok->color, max);
    *tok = b;
    *flat = b.color;
}

bool ThemeConfigNames(const ThemeConfig* cfg, const char* key) {
    // The aliases count: Rust's set is built from the config *struct* it
    // deserialized the file into, so a key the file spells the old way is in
    // it under the new name.
    return cfg && cfg->colors && FindColor(cfg->colors, key) != nullptr;
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

    SetToken(&out->background, &out->tokens.background,
             PickBg(c, "background", base.tokens.background));

    // The base colours, which the semantic ones fall back to, and their
    // `.light` halves — the background blended with 80% of the base where the
    // file does not name one. The colour picker shows all twelve.
    out->red = Pick(c, "base.red", base.red);
    out->green = Pick(c, "base.green", base.green);
    out->blue = Pick(c, "base.blue", base.blue);
    out->magenta = Pick(c, "base.magenta", base.magenta);
    out->yellow = Pick(c, "base.yellow", base.yellow);
    out->cyan = Pick(c, "base.cyan", base.cyan);
    out->redLight = Pick(c, "base.red.light",
                         Blend(out->background, RgbaOpacity(out->red, 0.8f)));
    out->greenLight =
        Pick(c, "base.green.light",
             Blend(out->background, RgbaOpacity(out->green, 0.8f)));
    out->blueLight = Pick(c, "base.blue.light",
                          Blend(out->background, RgbaOpacity(out->blue, 0.8f)));
    out->magentaLight =
        Pick(c, "base.magenta.light",
             Blend(out->background, RgbaOpacity(out->magenta, 0.8f)));
    out->yellowLight =
        Pick(c, "base.yellow.light",
             Blend(out->background, RgbaOpacity(out->yellow, 0.8f)));
    out->cyanLight = Pick(c, "base.cyan.light",
                          Blend(out->background, RgbaOpacity(out->cyan, 0.8f)));

    out->border = Pick(c, "border", base.border);
    out->foreground = Pick(c, "foreground", base.foreground);
    out->inputBorder = Pick(c, "input.border", out->border);
    SetToken(&out->muted, &out->tokens.muted,
             PickBg(c, "muted.background", base.tokens.muted));
    out->mutedFg = Pick(c, "muted.foreground",
                        Blend(out->muted, RgbaOpacity(out->foreground, 0.7f)));

    // Theme::input_background(): a dark input sits on its own border mixed
    // toward transparent, a light one on the window background.
    out->inputBg =
        dark ? MixOklab(out->inputBorder, clear, 0.3f) : out->background;

    SetToken(&out->primary, &out->tokens.primary,
             PickBg(c, "primary.background", base.tokens.primary));
    out->primaryFg = Pick(c, "primary.foreground", out->foreground);
    SetToken(&out->primaryHover, &out->tokens.primaryHover,
             PickBg(c, "primary.hover.background",
                    Blend(out->background,
                          RgbaOpacity(out->primary, hoverOpacity))));
    SetToken(&out->primaryActive, &out->tokens.primaryActive,
             PickBg(c, "primary.active.background",
                    Darken(out->primary, activeDarken)));

    SetToken(&out->secondary, &out->tokens.secondary,
             PickBg(c, "secondary.background", base.tokens.secondary));
    out->secondaryFg = Pick(c, "secondary.foreground", out->foreground);
    SetToken(&out->secondaryHover, &out->tokens.secondaryHover,
             PickBg(c, "secondary.hover.background",
                    Blend(out->background,
                          RgbaOpacity(out->secondary, hoverOpacity))));
    SetToken(&out->secondaryActive, &out->tokens.secondaryActive,
             PickBg(c, "secondary.active.background",
                    Darken(out->secondary, activeDarken)));

    SetToken(&out->success, &out->tokens.success,
             PickBg(c, "success.background", out->green));
    out->successFg = Pick(c, "success.foreground", out->primaryFg);
    SetToken(&out->info, &out->tokens.info,
             PickBg(c, "info.background", out->cyan));
    out->infoFg = Pick(c, "info.foreground", out->primaryFg);
    SetToken(&out->warning, &out->tokens.warning,
             PickBg(c, "warning.background", out->yellow));
    out->warningFg = Pick(c, "warning.foreground", out->primaryFg);

    SetToken(&out->accent, &out->tokens.accent,
             PickBg(c, "accent.background", out->tokens.secondary));
    out->accentFg = Pick(c, "accent.foreground", out->foreground);
    Rgba accentFg = out->accentFg;
    SetToken(&out->groupBox, &out->tokens.groupBox,
             PickBg(c, "group_box.background",
                    Blend(out->background,
                          RgbaOpacity(out->secondary, dark ? 0.3f : 0.4f))));
    out->groupBoxFg = Pick(c, "group_box.foreground", out->foreground);
    out->caret = Pick(c, "caret", out->primary);

    out->chart1 = Pick(c, "chart.1", Lighten(out->blue, 0.4f));
    out->chart2 = Pick(c, "chart.2", Lighten(out->blue, 0.2f));
    out->chart3 = Pick(c, "chart.3", out->blue);
    out->chart4 = Pick(c, "chart.4", Darken(out->blue, 0.2f));
    out->chart5 = Pick(c, "chart.5", Darken(out->blue, 0.4f));
    out->chartBullish = Pick(c, "chart_bullish", out->green);
    out->chartBearish = Pick(c, "chart_bearish", out->red);

    SetToken(&out->danger, &out->tokens.danger,
             PickBg(c, "danger.background", out->red));
    out->dangerFg = Pick(c, "danger.foreground", out->primaryFg);
    SetToken(&out->descListLabel, &out->tokens.descListLabel,
             PickBg(c, "description_list.label.background",
                    Blend(out->background, RgbaOpacity(out->border, 0.2f))));
    out->descListLabelFg =
        Pick(c, "description_list.label.foreground", out->mutedFg);
    out->dragBorder = Pick(c, "drag.border", RgbaOpacity(out->primary, 0.65f));

    SetToken(&out->list, &out->tokens.list,
             PickBg(c, "list.background", out->tokens.background));
    SetToken(&out->listActive, &out->tokens.listActive,
             PickBg(c, "list.active.background",
                    Blend(out->background, RgbaOpacity(out->primary, 0.1f))));
    out->listActiveBorder =
        Pick(c, "list.active.border",
             Blend(out->background, RgbaOpacity(out->primary, 0.6f)));
    SetToken(&out->listEven, &out->tokens.listEven,
             PickBg(c, "list.even.background", out->tokens.list));
    SetToken(&out->listHead, &out->tokens.listHead,
             PickBg(c, "list.head.background", out->tokens.list));
    SetToken(
        &out->listHover, &out->tokens.listHover,
        PickBg(c, "list.hover.background", RgbaOpacity(out->accent, 0.6f)));

    SetToken(&out->popover, &out->tokens.popover,
             PickBg(c, "popover.background", out->tokens.background));
    out->popoverFg = Pick(c, "popover.foreground", out->foreground);

    SetToken(&out->progress, &out->tokens.progress,
             PickBg(c, "progress.bar.background", out->tokens.primary));
    out->ring = Pick(c, "ring", out->blue);
    SetToken(&out->scrollbarThumb, &out->tokens.scrollbarThumb,
             PickBg(c, "scrollbar.thumb.background", out->tokens.accent));
    SetToken(&out->scrollbarThumbHover, &out->tokens.scrollbarThumbHover,
             PickBg(c, "scrollbar.thumb.hover.background",
                    out->tokens.scrollbarThumb));
    SetToken(&out->scrollbarBg, &out->tokens.scrollbarBg,
             PickBg(c, "scrollbar.background", out->tokens.background));
    SetToken(&out->selection, &out->tokens.selection,
             PickBg(c, "selection.background", out->tokens.primary));

    SetToken(&out->sidebar, &out->tokens.sidebar,
             PickBg(c, "sidebar.background",
                    Blend(out->background, RgbaOpacity(out->border, 0.15f))));
    SetToken(&out->sidebarAccent, &out->tokens.sidebarAccent,
             PickBg(c, "sidebar.accent.background", out->tokens.accent));
    out->sidebarAccentFg = Pick(c, "sidebar.accent.foreground", accentFg);
    out->sidebarBorder = Pick(c, "sidebar.border", out->border);
    out->sidebarFg = Pick(c, "sidebar.foreground", out->foreground);
    SetToken(&out->sidebarPrimary, &out->tokens.sidebarPrimary,
             PickBg(c, "sidebar.primary.background", out->tokens.primary));
    out->sidebarPrimaryFg =
        Pick(c, "sidebar.primary.foreground", out->primaryFg);

    SetToken(&out->skeleton, &out->tokens.skeleton,
             PickBg(c, "skeleton.background", out->tokens.secondary));
    SetToken(&out->tabActiveBg, &out->tokens.tabActiveBg,
             PickBg(c, "tab.active.background", out->tokens.background));
    out->tabActiveFg = Pick(c, "tab.active.foreground", out->foreground);
    SetToken(&out->tabBar, &out->tokens.tabBar,
             PickBg(c, "tab_bar.background", out->tokens.background));
    out->tabFg = Pick(c, "tab.foreground", out->foreground);

    SetToken(&out->tableBg, &out->tokens.tableBg,
             PickBg(c, "table.background", out->tokens.list));
    SetToken(&out->tableActive, &out->tokens.tableActive,
             PickBg(c, "table.active.background", out->tokens.listActive));
    out->tableActiveBorder =
        Pick(c, "table.active.border", out->listActiveBorder);
    SetToken(&out->tableEven, &out->tokens.tableEven,
             PickBg(c, "table.even.background", out->tokens.listEven));
    SetToken(&out->tableHead, &out->tokens.tableHead,
             PickBg(c, "table.head.background", out->tokens.listHead));
    out->tableHeadFg = Pick(c, "table.head.foreground", out->mutedFg);
    SetToken(&out->tableFoot, &out->tokens.tableFoot,
             PickBg(c, "table.foot.background", out->tokens.listHead));
    out->tableFootFg = Pick(c, "table.foot.foreground", out->mutedFg);
    out->tableRowBorder = Pick(c, "table.row.border", out->border);

    SetToken(&out->titleBar, &out->tokens.titleBar,
             PickBg(c, "title_bar.background", out->tokens.background));
    out->titleBarBorder = Pick(c, "title_bar.border", out->border);
    out->statusBarBorder = Pick(c, "status_bar.border", out->titleBarBorder);
    SetToken(&out->overlay, &out->tokens.overlay,
             PickBg(c, "overlay", base.tokens.overlay));

    // status_bar falls back to the title bar, and the switch and slider
    // thumbs to the window background — the three tokens this tree keeps only
    // so a theme that spells one as a gradient gets one.
    SetToken(&out->statusBar, &out->tokens.statusBar,
             PickBg(c, "status_bar.background", out->tokens.titleBar));
    SetToken(&out->switchThumb, &out->tokens.switchThumb,
             PickBg(c, "switch.thumb.background", out->tokens.background));
    SetToken(&out->sliderThumb, &out->tokens.sliderThumb,
             PickBg(c, "slider.thumb.background", out->tokens.background));

    // The rest of ThemeColor, each on the key schema.rs reads it from and the
    // fallback it falls back to. `ThemeFillDerived` has already put every one
    // of them on that fallback, so what is left here is the file's own word
    // over the top — read in schema.rs's order, since a few of them fall back
    // to one another.
    ThemeFillDerived(out, dark);

    SetToken(&out->button, &out->tokens.button,
             PickBg(c, "button.background", out->tokens.button));
    out->buttonFg = Pick(c, "button.foreground", out->foreground);
    SetToken(&out->buttonHover, &out->tokens.buttonHover,
             PickBg(c, "button.hover.background", out->tokens.buttonHover));
    SetToken(&out->buttonActive, &out->tokens.buttonActive,
             PickBg(c, "button.active.background", out->tokens.buttonActive));
    SetToken(&out->buttonPrimary, &out->tokens.buttonPrimary,
             PickBg(c, "button.primary.background", out->tokens.primary));
    out->buttonPrimaryFg = Pick(c, "button.primary.foreground", out->primaryFg);
    SetToken(
        &out->buttonPrimaryHover, &out->tokens.buttonPrimaryHover,
        PickBg(c, "button.primary.hover.background", out->tokens.primaryHover));
    SetToken(&out->buttonPrimaryActive, &out->tokens.buttonPrimaryActive,
             PickBg(c, "button.primary.active.background",
                    out->tokens.primaryActive));
    SetToken(&out->buttonSecondary, &out->tokens.buttonSecondary,
             PickBg(c, "button.secondary.background", out->tokens.secondary));
    out->buttonSecondaryFg =
        Pick(c, "button.secondary.foreground", out->secondaryFg);
    SetToken(&out->buttonSecondaryHover, &out->tokens.buttonSecondaryHover,
             PickBg(c, "button.secondary.hover.background",
                    out->tokens.secondaryHover));
    SetToken(&out->buttonSecondaryActive, &out->tokens.buttonSecondaryActive,
             PickBg(c, "button.secondary.active.background",
                    out->tokens.secondaryActive));

    SetToken(&out->successHover, &out->tokens.successHover,
             PickBg(c, "success.hover.background",
                    Blend(out->background,
                          RgbaOpacity(out->success, hoverOpacity))));
    SetToken(&out->successActive, &out->tokens.successActive,
             PickBg(c, "success.active.background",
                    Darken(out->success, activeDarken)));
    SetToken(&out->buttonSuccess, &out->tokens.buttonSuccess,
             PickBg(c, "button.success.background",
                    MixOklab(out->success, clear, 0.2f)));
    out->buttonSuccessFg = Pick(c, "button.success.foreground", out->success);
    SetToken(&out->buttonSuccessHover, &out->tokens.buttonSuccessHover,
             PickBg(c, "button.success.hover.background",
                    MixOklab(out->success, clear, 0.3f)));
    SetToken(&out->buttonSuccessActive, &out->tokens.buttonSuccessActive,
             PickBg(c, "button.success.active.background",
                    MixOklab(out->success, clear, 0.4f)));

    SetToken(
        &out->infoHover, &out->tokens.infoHover,
        PickBg(c, "info.hover.background",
               Blend(out->background, RgbaOpacity(out->info, hoverOpacity))));
    SetToken(
        &out->infoActive, &out->tokens.infoActive,
        PickBg(c, "info.active.background", Darken(out->info, activeDarken)));
    SetToken(
        &out->buttonInfo, &out->tokens.buttonInfo,
        PickBg(c, "button.info.background", MixOklab(out->info, clear, 0.2f)));
    out->buttonInfoFg = Pick(c, "button.info.foreground", out->info);
    SetToken(&out->buttonInfoHover, &out->tokens.buttonInfoHover,
             PickBg(c, "button.info.hover.background",
                    MixOklab(out->info, clear, 0.3f)));
    SetToken(&out->buttonInfoActive, &out->tokens.buttonInfoActive,
             PickBg(c, "button.info.active.background",
                    MixOklab(out->info, clear, 0.4f)));

    SetToken(&out->warningHover, &out->tokens.warningHover,
             PickBg(c, "warning.hover.background",
                    Blend(out->background,
                          RgbaOpacity(out->warning, hoverOpacity))));
    SetToken(
        &out->warningActive, &out->tokens.warningActive,
        PickBg(c, "warning.active.background",
               Blend(out->background, Darken(out->warning, activeDarken))));
    SetToken(&out->buttonWarning, &out->tokens.buttonWarning,
             PickBg(c, "button.warning.background",
                    MixOklab(out->warning, clear, 0.2f)));
    out->buttonWarningFg = Pick(c, "button.warning.foreground", out->warning);
    SetToken(&out->buttonWarningHover, &out->tokens.buttonWarningHover,
             PickBg(c, "button.warning.hover.background",
                    MixOklab(out->warning, clear, 0.3f)));
    SetToken(&out->buttonWarningActive, &out->tokens.buttonWarningActive,
             PickBg(c, "button.warning.active.background",
                    MixOklab(out->warning, clear, 0.4f)));

    SetToken(&out->dangerActive, &out->tokens.dangerActive,
             PickBg(c, "danger.active.background",
                    Darken(out->danger, activeDarken)));
    SetToken(
        &out->dangerHover, &out->tokens.dangerHover,
        PickBg(c, "danger.hover.background",
               Blend(out->background, RgbaOpacity(out->danger, hoverOpacity))));
    SetToken(&out->buttonDanger, &out->tokens.buttonDanger,
             PickBg(c, "button.danger.background",
                    MixOklab(out->danger, clear, 0.2f)));
    out->buttonDangerFg = Pick(c, "button.danger.foreground", out->danger);
    SetToken(&out->buttonDangerHover, &out->tokens.buttonDangerHover,
             PickBg(c, "button.danger.hover.background",
                    MixOklab(out->danger, clear, 0.3f)));
    SetToken(&out->buttonDangerActive, &out->tokens.buttonDangerActive,
             PickBg(c, "button.danger.active.background",
                    MixOklab(out->danger, clear, 0.4f)));

    SetToken(&out->accordion, &out->tokens.accordion,
             PickBg(c, "accordion.background", out->tokens.background));
    SetToken(
        &out->dropTarget, &out->tokens.dropTarget,
        PickBg(c, "drop_target.background", RgbaOpacity(out->primary, 0.2f)));
    out->link = Pick(c, "link", out->primary);
    out->linkActive = Pick(c, "link.active", out->link);
    out->linkHover = Pick(c, "link.hover", out->link);
    SetToken(&out->tableHover, &out->tokens.tableHover,
             PickBg(c, "table.hover.background", out->tokens.listHover));
    SetToken(&out->sliderBar, &out->tokens.sliderBar,
             PickBg(c, "slider.background", out->tokens.primary));
    SetToken(&out->switchBg, &out->tokens.switchBg,
             PickBg(c, "switch.background", out->tokens.secondaryActive));
    SetToken(&out->tab, &out->tokens.tab,
             PickBg(c, "tab.background", out->tokens.background));
    SetToken(&out->tabBarSegmented, &out->tokens.tabBarSegmented,
             PickBg(c, "tab_bar.segmented.background", out->tokens.secondary));
    SetToken(&out->tiles, &out->tokens.tiles,
             PickBg(c, "tiles.background", out->tokens.background));
    out->windowBorder = Pick(c, "window.border", out->border);

    // The three that are painted over text, capped however the file spells
    // them: a row highlight at a fifth, a text selection at a third.
    ClampToken(&out->listActive, &out->tokens.listActive,
               FindColor(c, "list.active.background") != nullptr, 0.2f);
    ClampToken(&out->tableActive, &out->tokens.tableActive,
               FindColor(c, "table.active.background") != nullptr, 0.2f);
    ClampToken(&out->selection, &out->tokens.selection,
               FindColor(c, "selection.background") != nullptr, 0.3f);

    // Everything the file did not spell as a gradient is its flat colour.
    ThemeTokensReset(out);

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
// The directories LoadDir has already read, so reading one twice is free.
// Callers treat LoadDir as "make sure these themes are in the registry" and
// one of them — the story's app menu — says it on every frame; without this
// each of those calls re-read and re-parsed every file in the directory into
// gArena, which nothing ever hands back.
static Vec<Str> gLoadedDirs;

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
    // What a theme keeps — its name, its colors object — points into the
    // document, so an accepted theme pins the parse. A document that adds
    // nothing pins nothing, and the arena goes back to where it was.
    uint64_t mark = ArenaUsed(gArena);
    JsonValue* doc = JsonParse(gArena, json);
    if (!doc) {
        gArena->PopTo(mark);
        return 0;
    }
    const JsonValue* themes = JsonGet(doc, "themes");
    if (!themes || themes->kind != JsonKind::Array) {
        gArena->PopTo(mark);
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
    if (added == 0) {
        gArena->PopTo(mark);
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

// ─── a theme written as semantic tokens ──────────────────────────────────
//
// `SemanticThemeConfigFile`: a document whose whole content is
// `{"tokens": {...}}`, in the vocabulary of `theme_tokens.rs` rather than the
// legacy key list. Rust resolves it over the theme in force — every field is
// an Option and what it leaves out stays as it was — and then applies the
// result back onto that theme. Same here, over the mode's current palette.
static void ApplyColorField(const JsonValue* obj, const char* key, Rgba* out) {
    Str v = JsonString(JsonGet(obj, key));
    Rgba c;
    if (v.s && ThemeParseColor(v, &c)) {
        *out = c;
    }
}

static void ApplyFloatField(const JsonValue* obj, const char* key, float* out) {
    const JsonValue* v = JsonGet(obj, key);
    if (v && v->kind == JsonKind::Number) {
        *out = (float)v->num;
    }
}

static void ApplyTextStyle(const JsonValue* obj, const char* key,
                           SemanticTextStyle* out) {
    const JsonValue* v = JsonGet(obj, key);
    if (!v || v->kind != JsonKind::Object) {
        return;
    }
    ApplyFloatField(v, "size", &out->size);
    ApplyFloatField(v, "line_height", &out->lineHeight);
    ApplyFloatField(v, "weight", &out->weight);
}

static void ApplyShadowLevel(const JsonValue* obj, const char* key,
                             SemanticShadow* out, bool* any) {
    const JsonValue* v = JsonGet(obj, key);
    if (!v || v->kind != JsonKind::Object) {
        return;
    }
    *any = true;
    ApplyFloatField(v, "x", &out->x);
    ApplyFloatField(v, "y", &out->y);
    ApplyFloatField(v, "blur", &out->blur);
    ApplyFloatField(v, "spread", &out->spread);
    ApplyColorField(v, "color", &out->color);
}

bool ThemeSemanticConfigApply(const JsonValue* doc, SemanticThemeTokens* io) {
    const JsonValue* tokens = JsonGet(doc, "tokens");
    if (!tokens || tokens->kind != JsonKind::Object) {
        return false;
    }
    if (const JsonValue* c = JsonGet(tokens, "colors")) {
        SemanticColorTokens& t = io->colors;
        ApplyColorField(c, "background", &t.background);
        ApplyColorField(c, "foreground", &t.foreground);
        ApplyColorField(c, "surface", &t.surface);
        ApplyColorField(c, "surface_foreground", &t.surfaceForeground);
        ApplyColorField(c, "primary", &t.primary);
        ApplyColorField(c, "primary_foreground", &t.primaryForeground);
        ApplyColorField(c, "secondary", &t.secondary);
        ApplyColorField(c, "secondary_foreground", &t.secondaryForeground);
        ApplyColorField(c, "muted", &t.muted);
        ApplyColorField(c, "muted_foreground", &t.mutedForeground);
        ApplyColorField(c, "accent", &t.accent);
        ApplyColorField(c, "accent_foreground", &t.accentForeground);
        ApplyColorField(c, "destructive", &t.destructive);
        ApplyColorField(c, "destructive_foreground", &t.destructiveForeground);
        ApplyColorField(c, "border", &t.border);
        ApplyColorField(c, "input", &t.input);
        ApplyColorField(c, "ring", &t.ring);
    }
    if (const JsonValue* r = JsonGet(tokens, "radius")) {
        ApplyFloatField(r, "none", &io->radius.none);
        ApplyFloatField(r, "sm", &io->radius.sm);
        ApplyFloatField(r, "md", &io->radius.md);
        ApplyFloatField(r, "lg", &io->radius.lg);
        ApplyFloatField(r, "xl", &io->radius.xl);
        ApplyFloatField(r, "full", &io->radius.full);
    }
    if (const JsonValue* sp = JsonGet(tokens, "spacing")) {
        ApplyFloatField(sp, "xxs", &io->spacing.xxs);
        ApplyFloatField(sp, "xs", &io->spacing.xs);
        ApplyFloatField(sp, "sm", &io->spacing.sm);
        ApplyFloatField(sp, "md", &io->spacing.md);
        ApplyFloatField(sp, "lg", &io->spacing.lg);
        ApplyFloatField(sp, "xl", &io->spacing.xl);
        ApplyFloatField(sp, "xxl", &io->spacing.xxl);
    }
    if (const JsonValue* ty = JsonGet(tokens, "typography")) {
        Str sans = JsonString(JsonGet(ty, "sans"));
        Str mono = JsonString(JsonGet(ty, "mono"));
        if (sans.s) {
            io->typography.sans = sans;
        }
        if (mono.s) {
            io->typography.mono = mono;
        }
        ApplyTextStyle(ty, "xs", &io->typography.xs);
        ApplyTextStyle(ty, "sm", &io->typography.sm);
        ApplyTextStyle(ty, "md", &io->typography.md);
        ApplyTextStyle(ty, "lg", &io->typography.lg);
        ApplyTextStyle(ty, "xl", &io->typography.xl);
        ApplyTextStyle(ty, "mono_md", &io->typography.monoMd);
    }
    if (const JsonValue* sh = JsonGet(tokens, "shadow")) {
        bool any = false;
        ApplyShadowLevel(sh, "sm", &io->shadow.sm, &any);
        ApplyShadowLevel(sh, "md", &io->shadow.md, &any);
        ApplyShadowLevel(sh, "lg", &io->shadow.lg, &any);
        if (any) {
            io->shadow.has = true;
        }
    }
    return true;
}

bool ThemeApplySemanticConfigStr(ThemeMode mode, Str json,
                                 SemanticThemeTokens* out) {
    if (!json.s || json.len <= 0) {
        return false;
    }
    ThemeRegistryInit();
    // The document is read and thrown away: nothing a semantic config holds
    // outlives the colours it is turned into.
    Arena* a = ArenaNew();
    JsonValue* doc = JsonParse(a, json);
    Theme t = mode == ThemeMode::Dark ? ThemeDark() : ThemeLight();
    SemanticThemeTokens tokens = ThemeSemanticTokens(t);
    bool ok = doc && ThemeSemanticConfigApply(doc, &tokens);
    // The two font families are the only strings a token set keeps, and they
    // point into the document. They move to the registry's own arena — where
    // every theme's name already lives — so the answer outlives the parse.
    if (ok && gArena) {
        tokens.typography.sans = StrDup(gArena, tokens.typography.sans);
        tokens.typography.mono = StrDup(gArena, tokens.typography.mono);
    }
    ArenaDelete(a);
    if (!ok) {
        return false;
    }
    ThemeApplySemanticTokens(&t, tokens);
    ThemeInstall(mode, t);
    if (out) {
        *out = tokens;
    }
    return true;
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
    // Already read once. A theme is never dropped, so a second pass over the
    // same directory can only find what is in the registry already.
    Str dirKey = Str(path);
    for (int i = 0; i < gLoadedDirs.len; i++) {
        if (SameName(gLoadedDirs[i], dirKey)) {
            return 0;
        }
    }
    gLoadedDirs.Append(StrDup(gArena, dirKey));
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
    gLoadedDirs.Reset();
    if (gArena) {
        ArenaDelete(gArena);
        gArena = nullptr;
    }
    gActive[0] = {};
    gActive[1] = {};
    gInited = false;
}

} // namespace gpui
