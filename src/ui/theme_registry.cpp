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
    uint8_t cap = (uint8_t)(Clamp01f(max) * 255.f + 0.5f);
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
    Rgba primaryHover =
        Pick(c, "primary.hover.background",
             Blend(out->background, RgbaOpacity(out->primary, hoverOpacity)));
    Rgba primaryActive = Pick(c, "primary.active.background",
                              Darken(out->primary, activeDarken));
    (void)primaryHover;
    (void)primaryActive;

    SetToken(&out->secondary, &out->tokens.secondary,
             PickBg(c, "secondary.background", base.tokens.secondary));
    out->secondaryFg = Pick(c, "secondary.foreground", out->foreground);
    out->secondaryHover =
        Pick(c, "secondary.hover.background",
             Blend(out->background, RgbaOpacity(out->secondary, hoverOpacity)));
    out->secondaryActive = Pick(c, "secondary.active.background",
                                Darken(out->secondary, activeDarken));

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

    SetToken(&out->danger, &out->tokens.danger,
             PickBg(c, "danger.background", out->red));
    out->dangerFg = Pick(c, "danger.foreground", out->primaryFg);
    out->descListLabel =
        Pick(c, "description_list.label.background",
             Blend(out->background, RgbaOpacity(out->border, 0.2f)));
    out->descListLabelFg =
        Pick(c, "description_list.label.foreground", out->mutedFg);
    out->dragBorder = Pick(c, "drag.border", RgbaOpacity(out->primary, 0.65f));

    Background list = PickBg(c, "list.background", out->tokens.background);
    SetToken(&out->listActive, &out->tokens.listActive,
             PickBg(c, "list.active.background",
                    Blend(out->background, RgbaOpacity(out->primary, 0.1f))));
    out->listActiveBorder =
        Pick(c, "list.active.border",
             Blend(out->background, RgbaOpacity(out->primary, 0.6f)));
    Background listEven = PickBg(c, "list.even.background", list);
    Background listHead = PickBg(c, "list.head.background", list);

    SetToken(&out->progress, &out->tokens.progress,
             PickBg(c, "progress.bar.background", out->tokens.primary));
    out->ring = Pick(c, "ring", out->blue);
    SetToken(&out->scrollbarThumb, &out->tokens.scrollbarThumb,
             PickBg(c, "scrollbar.thumb.background", out->tokens.accent));
    SetToken(&out->selection, &out->tokens.selection,
             PickBg(c, "selection.background", out->tokens.primary));

    out->sidebar =
        Pick(c, "sidebar.background",
             Blend(out->background, RgbaOpacity(out->border, 0.15f)));
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
             PickBg(c, "table.background", list));
    SetToken(&out->tableActive, &out->tokens.tableActive,
             PickBg(c, "table.active.background", out->tokens.listActive));
    out->tableActiveBorder =
        Pick(c, "table.active.border", out->listActiveBorder);
    SetToken(&out->tableEven, &out->tokens.tableEven,
             PickBg(c, "table.even.background", listEven));
    SetToken(&out->tableHead, &out->tokens.tableHead,
             PickBg(c, "table.head.background", listHead));
    out->tableHeadFg = Pick(c, "table.head.foreground", out->mutedFg);
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
