#include "gpui/gpui.h"
#include "gpui/keymap.h"
#include "base/scrollbar.h"
#include "gpui/image.h"
#include "gpui/paint.h"
#include "base/positioner.h"
#include "base/text_boundary.h"
#include "gpui/svg.h"

#include <math.h>

// ─── color / theme ────────────────────────────────────────────────────────

namespace gpui {

// The float→byte rule the whole palette is written to: truncate, which is
// what `(rgb.r * 255.) as u32` does in `Colorize::to_hex` and everywhere else
// Rust turns one of its float colours into a byte. Rust keeps a colour as
// four floats and only quantises when it prints or paints; this tree keeps
// bytes, so the quantisation happens at every step — rounding it up half the
// time left the hex the theme viewer and the colour picker print one above
// the number Rust prints for the same colour.
static uint8_t ToByte(float v01) {
    if (v01 <= 0) {
        return 0;
    }
    return v01 >= 1.f ? 255 : (uint8_t)(v01 * 255.f);
}

Rgba RgbaOpacity(Rgba c, float a01) {
    if (a01 < 0) {
        a01 = 0;
    }
    if (a01 > 1) {
        a01 = 1;
    }
    c.a = (uint8_t)(c.a * a01);
    return c;
}

Rgba RgbaMix(Rgba a, Rgba b, float t) {
    if (t < 0) {
        t = 0;
    }
    if (t > 1) {
        t = 1;
    }
    Rgba o;
    o.r = (uint8_t)(a.r * t + b.r * (1 - t) + 0.5f);
    o.g = (uint8_t)(a.g * t + b.g * (1 - t) + 0.5f);
    o.b = (uint8_t)(a.b * t + b.b * (1 - t) + 0.5f);
    o.a = (uint8_t)(a.a * t + b.a * (1 - t) + 0.5f);
    return o;
}

static float Clamp01(float v) {
    if (v < 0) {
        return 0;
    }
    return v > 1 ? 1 : v;
}

Rgba RgbaHsla(float h, float s, float l, float a01) {
    h = h - floorf(h); // hue wraps, everything else clamps
    s = Clamp01(s);
    l = Clamp01(l);
    float c = (1.f - fabsf(2.f * l - 1.f)) * s;
    float hp = h * 6.f;
    float x = c * (1.f - fabsf(fmodf(hp, 2.f) - 1.f));
    float r = 0, g = 0, b = 0;
    if (hp < 1.f) {
        r = c;
        g = x;
    } else if (hp < 2.f) {
        r = x;
        g = c;
    } else if (hp < 3.f) {
        g = c;
        b = x;
    } else if (hp < 4.f) {
        g = x;
        b = c;
    } else if (hp < 5.f) {
        r = x;
        b = c;
    } else {
        r = c;
        b = x;
    }
    float m = l - c * 0.5f;
    return Rgba{ToByte(r + m), ToByte(g + m), ToByte(b + m), ToByte(a01)};
}

void RgbaToHsla(Rgba c, float* h, float* s, float* l) {
    float r = c.r / 255.f, g = c.g / 255.f, b = c.b / 255.f;
    float mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
    float mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
    float d = mx - mn;
    float hh = 0;
    if (d > 0) {
        if (mx == r) {
            hh = fmodf((g - b) / d, 6.f);
        } else if (mx == g) {
            hh = (b - r) / d + 2.f;
        } else {
            hh = (r - g) / d + 4.f;
        }
        hh /= 6.f;
        if (hh < 0) {
            hh += 1.f;
        }
    }
    float ll = (mx + mn) * 0.5f;
    float ss = 0;
    if (d > 0) {
        float den = 1.f - fabsf(2.f * ll - 1.f);
        ss = den > 0 ? d / den : 0;
    }
    *h = hh;
    *s = ss;
    *l = ll;
}

Rgba RgbaWithHue(Rgba c, float h01) {
    float h = 0, s = 0, l = 0;
    RgbaToHsla(c, &h, &s, &l);
    return RgbaHsla(Clamp01(h01), s, l, c.a / 255.f);
}

// ─── background ───────────────────────────────────────────────────────────

Background BackgroundLinear(float angle, ColorStop from, ColorStop to) {
    Background b;
    // try_parse_theme_color: the flat colour a gradient stands in for is its
    // first stop, so a caller that only wants one colour gets a sensible one.
    b.color = from.color;
    b.from = from;
    b.to = to;
    b.angle = angle;
    b.gradient = true;
    return b;
}

Background BackgroundOpacity(Background b, float factor) {
    // RgbaOpacity scales what is already there, the way Colorize::opacity
    // does, so the two stops keep their ratio.
    b.color = RgbaOpacity(b.color, factor);
    if (b.gradient) {
        b.from.color = RgbaOpacity(b.from.color, factor);
        b.to.color = RgbaOpacity(b.to.color, factor);
    }
    return b;
}

// Hsla::alpha(a.min(max)): a ceiling, not a scale.
static Rgba CapAlpha(Rgba c, float max) {
    uint8_t cap = ToByte(max);
    if (c.a > cap) {
        c.a = cap;
    }
    return c;
}

Background BackgroundClampAlpha(Background b, float max) {
    b.color = CapAlpha(b.color, max);
    if (b.gradient) {
        b.from.color = CapAlpha(b.from.color, max);
        b.to.color = CapAlpha(b.to.color, max);
    }
    return b;
}

void BackgroundLine(const Background& b, Bounds box, Point* p0, Point* p1) {
    if (!p0 || !p1) {
        return;
    }
    float rad = b.angle * (kPi / 180.f);
    // CSS measures from "to top" and turns clockwise; y grows downward here,
    // so the direction the gradient runs in is (sin, -cos).
    float dx = sinf(rad);
    float dy = -cosf(rad);
    // The line is long enough for the corners to fall on its ends: the box
    // projected onto the direction.
    float len = fabsf(box.w * dx) + fabsf(box.h * dy);
    float cx = box.CenterX(), cy = box.CenterY();
    float sx = cx - dx * len * 0.5f, sy = cy - dy * len * 0.5f;
    p0->x = sx + dx * len * b.from.percentage;
    p0->y = sy + dy * len * b.from.percentage;
    p1->x = sx + dx * len * b.to.percentage;
    p1->y = sy + dy * len * b.to.percentage;
}

// ─── Colorize — crates/ui/src/theme/color.rs ─────────────────────────────

// gpui::transparent_black(), which every `mix_oklab` toward nothing takes.
Rgba RgbaTransparent() {
    return Rgba8(0, 0, 0, 0);
}

// gpui::Hsla::blend: `over` composited onto `base` by its own alpha. The
// result keeps the base's alpha, which is why `background.blend(x)` is opaque
// however faint `x` is.
Rgba RgbaBlend(Rgba base, Rgba over) {
    if (over.a >= 255) {
        return over;
    }
    if (over.a == 0) {
        return base;
    }
    float f = over.a / 255.f;
    auto mix = [&](uint8_t b, uint8_t o) {
        return (uint8_t)(b * (1.f - f) + o * f);
    };
    return Rgba8(mix(base.r, over.r), mix(base.g, over.g), mix(base.b, over.b),
                 base.a);
}

// Colorize::lighten / ::darken, which scale the HSL lightness rather than
// mixing toward white or black.
static Rgba ScaleLightness(Rgba c, float factor) {
    float h = 0, s = 0, l = 0;
    RgbaToHsla(c, &h, &s, &l);
    return RgbaHsla(h, s, Clamp01(l * factor), c.a / 255.f);
}

Rgba RgbaLighten(Rgba c, float amount) {
    return ScaleLightness(c, 1.f + Clamp01(amount));
}

Rgba RgbaDarken(Rgba c, float amount) {
    return ScaleLightness(c, 1.f - Clamp01(amount));
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
    auto b8 = [](float v) { return ToByte(FromLinear(v)); };
    return Rgba8(b8(lr), b8(lg), b8(lb), ToByte(alpha));
}

// Colorize::mix_oklab, which is CSS `color-mix(in oklab, a factor%, b)`: the
// alpha is interpolated first and the Oklab channels are premultiplied by it,
// so mixing toward transparent fades without dragging the hue to black.
Rgba RgbaMixOklab(Rgba a, Rgba b, float factor) {
    factor = Clamp01(factor);
    float inv = 1.f - factor;
    float aa = a.a / 255.f, ab = b.a / 255.f;
    float alpha = aa * factor + ab * inv;
    if (alpha <= 0) {
        return RgbaTransparent();
    }
    // When one side is fully transparent it contributes nothing to the
    // premultiplied sum, so the mix is the other side's colour exactly and
    // only the alpha moves. Rust keeps its channels in f32 and never notices;
    // an `Rgba` here is eight bits a channel, so the `* aa * factor / alpha`
    // round trip through Oklab and back can land a channel on the far side of
    // a rounding boundary — which is a byte of drift that depends on the
    // platform's `cbrtf` and `powf`, not on the colour. Say what the maths
    // says instead.
    if (ab <= 0) {
        return Rgba8(a.r, a.g, a.b, ToByte(alpha));
    }
    if (aa <= 0) {
        return Rgba8(b.r, b.g, b.b, ToByte(alpha));
    }
    float l1, a1, b1, l2, a2, b2;
    RgbToOklab(a, &l1, &a1, &b1);
    RgbToOklab(b, &l2, &a2, &b2);
    float L = (l1 * aa * factor + l2 * ab * inv) / alpha;
    float A = (a1 * aa * factor + a2 * ab * inv) / alpha;
    float B = (b1 * aa * factor + b2 * ab * inv) / alpha;
    return OklabToRgb(L, A, B, alpha);
}

Str RgbaToHex(Arena* a, Rgba c, bool upper) {
    float h = 0, s = 0, l = 0;
    RgbaToHsla(c, &h, &s, &l);
    Rgba p = RgbaHsla(h, s, l, c.a / 255.f);
    if (c.a < 255) {
        return StrDup(a, upper ? fmt("#%02X%02X%02X%02X", p.r, p.g, p.b, p.a)
                               : fmt("#%02x%02x%02x%02x", p.r, p.g, p.b, p.a));
    }
    return StrDup(a, upper ? fmt("#%02X%02X%02X", p.r, p.g, p.b)
                           : fmt("#%02x%02x%02x", p.r, p.g, p.b));
}

void ThemeTokensReset(Theme* t) {
    if (!t) {
        return;
    }
    if (!t->tokens.background.gradient) {
        t->tokens.background = t->background;
    }
    if (!t->tokens.titleBar.gradient) {
        t->tokens.titleBar = t->titleBar;
    }
    if (!t->tokens.statusBar.gradient) {
        t->tokens.statusBar = t->statusBar;
    }
    if (!t->tokens.tabBar.gradient) {
        t->tokens.tabBar = t->tabBar;
    }
    if (!t->tokens.tabActiveBg.gradient) {
        t->tokens.tabActiveBg = t->tabActiveBg;
    }
    if (!t->tokens.primary.gradient) {
        t->tokens.primary = t->primary;
    }
    if (!t->tokens.secondary.gradient) {
        t->tokens.secondary = t->secondary;
    }
    if (!t->tokens.accent.gradient) {
        t->tokens.accent = t->accent;
    }
    if (!t->tokens.muted.gradient) {
        t->tokens.muted = t->muted;
    }
    if (!t->tokens.danger.gradient) {
        t->tokens.danger = t->danger;
    }
    if (!t->tokens.info.gradient) {
        t->tokens.info = t->info;
    }
    if (!t->tokens.success.gradient) {
        t->tokens.success = t->success;
    }
    if (!t->tokens.warning.gradient) {
        t->tokens.warning = t->warning;
    }
    if (!t->tokens.progress.gradient) {
        t->tokens.progress = t->progress;
    }
    if (!t->tokens.popover.gradient) {
        t->tokens.popover = t->popover;
    }
    if (!t->tokens.scrollbarThumb.gradient) {
        t->tokens.scrollbarThumb = t->scrollbarThumb;
    }
    if (!t->tokens.scrollbarThumbHover.gradient) {
        t->tokens.scrollbarThumbHover = t->scrollbarThumbHover;
    }
    if (!t->tokens.skeleton.gradient) {
        t->tokens.skeleton = t->skeleton;
    }
    if (!t->tokens.selection.gradient) {
        t->tokens.selection = t->selection;
    }
    if (!t->tokens.listActive.gradient) {
        t->tokens.listActive = t->listActive;
    }
    if (!t->tokens.tableBg.gradient) {
        t->tokens.tableBg = t->tableBg;
    }
    if (!t->tokens.tableActive.gradient) {
        t->tokens.tableActive = t->tableActive;
    }
    if (!t->tokens.tableEven.gradient) {
        t->tokens.tableEven = t->tableEven;
    }
    if (!t->tokens.tableHead.gradient) {
        t->tokens.tableHead = t->tableHead;
    }
    if (!t->tokens.tableFoot.gradient) {
        t->tokens.tableFoot = t->tableFoot;
    }
    if (!t->tokens.sidebarAccent.gradient) {
        t->tokens.sidebarAccent = t->sidebarAccent;
    }
    if (!t->tokens.sidebarPrimary.gradient) {
        t->tokens.sidebarPrimary = t->sidebarPrimary;
    }
    if (!t->tokens.overlay.gradient) {
        t->tokens.overlay = t->overlay;
    }
    if (!t->tokens.switchThumb.gradient) {
        t->tokens.switchThumb = t->switchThumb;
    }
    if (!t->tokens.sliderThumb.gradient) {
        t->tokens.sliderThumb = t->sliderThumb;
    }
    if (!t->tokens.button.gradient) {
        t->tokens.button = t->button;
    }
    if (!t->tokens.buttonHover.gradient) {
        t->tokens.buttonHover = t->buttonHover;
    }
    if (!t->tokens.buttonActive.gradient) {
        t->tokens.buttonActive = t->buttonActive;
    }
    if (!t->tokens.primaryHover.gradient) {
        t->tokens.primaryHover = t->primaryHover;
    }
    if (!t->tokens.primaryActive.gradient) {
        t->tokens.primaryActive = t->primaryActive;
    }
    if (!t->tokens.buttonPrimary.gradient) {
        t->tokens.buttonPrimary = t->buttonPrimary;
    }
    if (!t->tokens.buttonPrimaryHover.gradient) {
        t->tokens.buttonPrimaryHover = t->buttonPrimaryHover;
    }
    if (!t->tokens.buttonPrimaryActive.gradient) {
        t->tokens.buttonPrimaryActive = t->buttonPrimaryActive;
    }
    if (!t->tokens.secondaryHover.gradient) {
        t->tokens.secondaryHover = t->secondaryHover;
    }
    if (!t->tokens.secondaryActive.gradient) {
        t->tokens.secondaryActive = t->secondaryActive;
    }
    if (!t->tokens.buttonSecondary.gradient) {
        t->tokens.buttonSecondary = t->buttonSecondary;
    }
    if (!t->tokens.buttonSecondaryHover.gradient) {
        t->tokens.buttonSecondaryHover = t->buttonSecondaryHover;
    }
    if (!t->tokens.buttonSecondaryActive.gradient) {
        t->tokens.buttonSecondaryActive = t->buttonSecondaryActive;
    }
    if (!t->tokens.successHover.gradient) {
        t->tokens.successHover = t->successHover;
    }
    if (!t->tokens.successActive.gradient) {
        t->tokens.successActive = t->successActive;
    }
    if (!t->tokens.buttonSuccess.gradient) {
        t->tokens.buttonSuccess = t->buttonSuccess;
    }
    if (!t->tokens.buttonSuccessHover.gradient) {
        t->tokens.buttonSuccessHover = t->buttonSuccessHover;
    }
    if (!t->tokens.buttonSuccessActive.gradient) {
        t->tokens.buttonSuccessActive = t->buttonSuccessActive;
    }
    if (!t->tokens.infoHover.gradient) {
        t->tokens.infoHover = t->infoHover;
    }
    if (!t->tokens.infoActive.gradient) {
        t->tokens.infoActive = t->infoActive;
    }
    if (!t->tokens.buttonInfo.gradient) {
        t->tokens.buttonInfo = t->buttonInfo;
    }
    if (!t->tokens.buttonInfoHover.gradient) {
        t->tokens.buttonInfoHover = t->buttonInfoHover;
    }
    if (!t->tokens.buttonInfoActive.gradient) {
        t->tokens.buttonInfoActive = t->buttonInfoActive;
    }
    if (!t->tokens.warningHover.gradient) {
        t->tokens.warningHover = t->warningHover;
    }
    if (!t->tokens.warningActive.gradient) {
        t->tokens.warningActive = t->warningActive;
    }
    if (!t->tokens.buttonWarning.gradient) {
        t->tokens.buttonWarning = t->buttonWarning;
    }
    if (!t->tokens.buttonWarningHover.gradient) {
        t->tokens.buttonWarningHover = t->buttonWarningHover;
    }
    if (!t->tokens.buttonWarningActive.gradient) {
        t->tokens.buttonWarningActive = t->buttonWarningActive;
    }
    if (!t->tokens.dangerHover.gradient) {
        t->tokens.dangerHover = t->dangerHover;
    }
    if (!t->tokens.dangerActive.gradient) {
        t->tokens.dangerActive = t->dangerActive;
    }
    if (!t->tokens.buttonDanger.gradient) {
        t->tokens.buttonDanger = t->buttonDanger;
    }
    if (!t->tokens.buttonDangerHover.gradient) {
        t->tokens.buttonDangerHover = t->buttonDangerHover;
    }
    if (!t->tokens.buttonDangerActive.gradient) {
        t->tokens.buttonDangerActive = t->buttonDangerActive;
    }
    if (!t->tokens.accordion.gradient) {
        t->tokens.accordion = t->accordion;
    }
    if (!t->tokens.dropTarget.gradient) {
        t->tokens.dropTarget = t->dropTarget;
    }
    if (!t->tokens.list.gradient) {
        t->tokens.list = t->list;
    }
    if (!t->tokens.listEven.gradient) {
        t->tokens.listEven = t->listEven;
    }
    if (!t->tokens.listHead.gradient) {
        t->tokens.listHead = t->listHead;
    }
    if (!t->tokens.listHover.gradient) {
        t->tokens.listHover = t->listHover;
    }
    if (!t->tokens.sliderBar.gradient) {
        t->tokens.sliderBar = t->sliderBar;
    }
    if (!t->tokens.switchBg.gradient) {
        t->tokens.switchBg = t->switchBg;
    }
    if (!t->tokens.tab.gradient) {
        t->tokens.tab = t->tab;
    }
    if (!t->tokens.tabBarSegmented.gradient) {
        t->tokens.tabBarSegmented = t->tabBarSegmented;
    }
    if (!t->tokens.tableHover.gradient) {
        t->tokens.tableHover = t->tableHover;
    }
    if (!t->tokens.tiles.gradient) {
        t->tokens.tiles = t->tiles;
    }
    if (!t->tokens.scrollbarBg.gradient) {
        t->tokens.scrollbarBg = t->scrollbarBg;
    }
    if (!t->tokens.sidebar.gradient) {
        t->tokens.sidebar = t->sidebar;
    }
    if (!t->tokens.groupBox.gradient) {
        t->tokens.groupBox = t->groupBox;
    }
    if (!t->tokens.descListLabel.gradient) {
        t->tokens.descListLabel = t->descListLabel;
    }
}

// ThemeFillDerived — the half of schema.rs's chain that is arithmetic rather
// than a key. Every expression here is the `fallback =` of the
// `apply_color!` / `apply_background_color!` with the same name, in the same
// order, so a palette in code and a theme file that names nothing land on the
// same numbers.
void ThemeFillDerived(Theme* t, bool dark) {
    if (!t) {
        return;
    }
    // The two constants the button and hover fallbacks are written against.
    const float activeDarken = dark ? 0.2f : 0.1f;
    const float hoverOpacity = 0.9f;
    const Rgba clear = RgbaTransparent();
    auto set = [](Rgba* flat, Background* tok, Rgba c) {
        *flat = c;
        *tok = c;
    };

    // Button. The plain one sits on the input border mixed toward
    // transparent in dark and on the window background in light.
    set(&t->button, &t->tokens.button,
        dark ? RgbaMixOklab(t->inputBorder, clear, 0.3f) : t->background);
    t->buttonFg = t->foreground;
    set(&t->buttonHover, &t->tokens.buttonHover,
        RgbaMixOklab(t->inputBorder, clear, 0.5f));
    set(&t->buttonActive, &t->tokens.buttonActive,
        RgbaMixOklab(t->inputBorder, clear, 0.7f));

    set(&t->buttonPrimary, &t->tokens.buttonPrimary, t->primary);
    t->buttonPrimaryFg = t->primaryFg;
    set(&t->buttonPrimaryHover, &t->tokens.buttonPrimaryHover, t->primaryHover);
    set(&t->buttonPrimaryActive, &t->tokens.buttonPrimaryActive,
        t->primaryActive);

    set(&t->buttonSecondary, &t->tokens.buttonSecondary, t->secondary);
    t->buttonSecondaryFg = t->secondaryFg;
    set(&t->buttonSecondaryHover, &t->tokens.buttonSecondaryHover,
        t->secondaryHover);
    set(&t->buttonSecondaryActive, &t->tokens.buttonSecondaryActive,
        t->secondaryActive);

    // The four semantic surfaces: a hover blended over the window, an active
    // one darkened, and a button family mixed toward transparent.
    set(&t->successHover, &t->tokens.successHover,
        RgbaBlend(t->background, RgbaOpacity(t->success, hoverOpacity)));
    set(&t->successActive, &t->tokens.successActive,
        RgbaDarken(t->success, activeDarken));
    set(&t->buttonSuccess, &t->tokens.buttonSuccess,
        RgbaMixOklab(t->success, clear, 0.2f));
    t->buttonSuccessFg = t->success;
    set(&t->buttonSuccessHover, &t->tokens.buttonSuccessHover,
        RgbaMixOklab(t->success, clear, 0.3f));
    set(&t->buttonSuccessActive, &t->tokens.buttonSuccessActive,
        RgbaMixOklab(t->success, clear, 0.4f));

    set(&t->infoHover, &t->tokens.infoHover,
        RgbaBlend(t->background, RgbaOpacity(t->info, hoverOpacity)));
    set(&t->infoActive, &t->tokens.infoActive,
        RgbaDarken(t->info, activeDarken));
    set(&t->buttonInfo, &t->tokens.buttonInfo,
        RgbaMixOklab(t->info, clear, 0.2f));
    t->buttonInfoFg = t->info;
    set(&t->buttonInfoHover, &t->tokens.buttonInfoHover,
        RgbaMixOklab(t->info, clear, 0.3f));
    set(&t->buttonInfoActive, &t->tokens.buttonInfoActive,
        RgbaMixOklab(t->info, clear, 0.4f));

    set(&t->warningHover, &t->tokens.warningHover,
        RgbaBlend(t->background, RgbaOpacity(t->warning, hoverOpacity)));
    // The one that is not a plain darken: warning's active is blended over
    // the window as well, which is what schema.rs writes.
    set(&t->warningActive, &t->tokens.warningActive,
        RgbaBlend(t->background, RgbaDarken(t->warning, activeDarken)));
    set(&t->buttonWarning, &t->tokens.buttonWarning,
        RgbaMixOklab(t->warning, clear, 0.2f));
    t->buttonWarningFg = t->warning;
    set(&t->buttonWarningHover, &t->tokens.buttonWarningHover,
        RgbaMixOklab(t->warning, clear, 0.3f));
    set(&t->buttonWarningActive, &t->tokens.buttonWarningActive,
        RgbaMixOklab(t->warning, clear, 0.4f));

    set(&t->dangerActive, &t->tokens.dangerActive,
        RgbaDarken(t->danger, activeDarken));
    set(&t->dangerHover, &t->tokens.dangerHover,
        RgbaBlend(t->background, RgbaOpacity(t->danger, hoverOpacity)));
    set(&t->buttonDanger, &t->tokens.buttonDanger,
        RgbaMixOklab(t->danger, clear, 0.2f));
    t->buttonDangerFg = t->danger;
    set(&t->buttonDangerHover, &t->tokens.buttonDangerHover,
        RgbaMixOklab(t->danger, clear, 0.3f));
    set(&t->buttonDangerActive, &t->tokens.buttonDangerActive,
        RgbaMixOklab(t->danger, clear, 0.4f));

    set(&t->accordion, &t->tokens.accordion, t->background);
    set(&t->dropTarget, &t->tokens.dropTarget, RgbaOpacity(t->primary, 0.2f));
    t->link = t->primary;
    t->linkActive = t->link;
    t->linkHover = t->link;
    set(&t->list, &t->tokens.list, t->background);
    set(&t->listEven, &t->tokens.listEven, t->list);
    set(&t->listHead, &t->tokens.listHead, t->list);
    set(&t->listHover, &t->tokens.listHover, RgbaOpacity(t->accent, 0.6f));
    set(&t->tableHover, &t->tokens.tableHover, t->listHover);
    set(&t->sliderBar, &t->tokens.sliderBar, t->primary);
    set(&t->switchBg, &t->tokens.switchBg, t->secondaryActive);
    set(&t->tab, &t->tokens.tab, t->background);
    set(&t->tabBarSegmented, &t->tokens.tabBarSegmented, t->secondary);
    set(&t->tiles, &t->tokens.tiles, t->background);
    t->windowBorder = t->border;
}

const Theme& ThemeDefaultDark() {
    static Theme t;
    static bool init = false;
    if (!init) {
        t.background = Rgb(0x0a, 0x0a, 0x0a);
        t.foreground = Rgb(0xfa, 0xfa, 0xfa);
        t.border = Rgb(0x26, 0x26, 0x26);
        t.mutedFg = Rgb(0xa3, 0xa3, 0xa3);
        t.inputBorder = Rgb(0x2f, 0x2f, 0x2f);
        t.inputBg = Rgba8(0x2f, 0x2f, 0x2f, 0x4c);
        t.ring = Rgb(0x73, 0x73, 0x73);
        t.caret = Rgb(0xfa, 0xfa, 0xfa);
        t.selection = Rgba8(0x1d, 0x4e, 0xd8, 0x4c);
        t.dragBorder = Rgb(0x3b, 0x82, 0xf6);
        t.titleBar = Rgb(0x17, 0x17, 0x17);
        t.titleBarBorder = Rgb(0x26, 0x26, 0x26);
        t.statusBarBorder = Rgb(0x26, 0x26, 0x26);
        t.tabBar = Rgb(0x17, 0x17, 0x17);
        t.tabActiveBg = Rgb(0x0a, 0x0a, 0x0a);
        t.tabActiveFg = Rgb(0xfa, 0xfa, 0xfa);
        t.tabFg = Rgb(0xd4, 0xd4, 0xd4);
        t.tableBg = Rgb(0x0a, 0x0a, 0x0a);
        t.tableHead = Rgba8(0x17, 0x17, 0x17, 0x66);
        t.tableHeadFg = Rgb(0x52, 0x52, 0x52);
        // table.foot has no entry of its own, so it takes list.head's
        // surface and muted_foreground, as schema.rs falls back.
        t.tableFoot = Rgba8(0x17, 0x17, 0x17, 0x66);
        t.tableFootFg = t.mutedFg;
        t.tableRowBorder = Rgba8(0x26, 0x26, 0x26, 0xb3);
        t.tableEven = Rgba8(0x17, 0x17, 0x17, 0x66);
        // default-theme.json dark: list.active.background #1e40af33,
        // list.active.border #1d4ed8. table.active has no entry of its own, so
        // it falls back to the list pair.
        t.listActive = Rgba8(0x1e, 0x40, 0xaf, 0x33);
        t.listActiveBorder = Rgb(0x1d, 0x4e, 0xd8);
        t.tableActive = t.listActive;
        t.tableActiveBorder = t.listActiveBorder;
        t.progress = Rgb(0xf5, 0xf5, 0xf5);
        t.red = Rgb(0xf8, 0x71, 0x71);
        t.green = Rgb(0x4a, 0xde, 0x80);
        t.blue = Rgb(0x60, 0xa5, 0xfa);
        t.yellow = Rgb(0xfa, 0xcc, 0x15);
        t.cyan = Rgb(0x22, 0xd3, 0xee);
        t.magenta = Rgb(0xc0, 0x84, 0xfc);
        // base.<hue>.light, one scale step lighter than the base in the
        // dark theme and one step lighter than the 600 in the light one.
        t.redLight = Rgb(0xfc, 0xa5, 0xa5);
        t.greenLight = Rgb(0x86, 0xef, 0xac);
        t.blueLight = Rgb(0x93, 0xc5, 0xfd);
        t.yellowLight = Rgb(0xfd, 0xe0, 0x47);
        t.cyanLight = Rgb(0x67, 0xe8, 0xf9);
        t.magentaLight = Rgb(0xd8, 0xb4, 0xfe);
        t.chart1 = Rgb(0x93, 0xc5, 0xfd);
        t.chart2 = Rgb(0x3b, 0x82, 0xf6);
        t.chart3 = Rgb(0x25, 0x63, 0xeb);
        t.chart4 = Rgb(0x1d, 0x4e, 0xd8);
        t.chart5 = Rgb(0x1e, 0x40, 0xaf);
        t.chartBullish = Rgb(0x16, 0xa3, 0x4a);
        t.chartBearish = Rgb(0xdc, 0x26, 0x26);
        t.danger = Rgb(0xf8, 0x71, 0x71);
        t.dangerFg = Rgb(0xdc, 0x26, 0x26);
        t.secondaryHover = Rgb(0x29, 0x29, 0x29);
        t.secondaryActive = Rgb(0x21, 0x21, 0x21);
        t.secondaryFg = Rgb(0xfa, 0xfa, 0xfa);
        t.secondary = Rgb(0x26, 0x26, 0x26);
        t.muted = Rgb(0x26, 0x26, 0x26);
        t.accent = Rgb(0x26, 0x26, 0x26);
        t.accentFg = Rgb(0xfa, 0xfa, 0xfa);
        t.primary = Rgb(0xfa, 0xfa, 0xfa);
        t.primaryFg = Rgb(0x17, 0x17, 0x17);
        t.primaryHover = Rgb(0xf5, 0xf5, 0xf5);
        t.primaryActive = Rgb(0xe5, 0xe5, 0xe5);
        t.sidebar = Rgb(0x0a, 0x0a, 0x0a);
        t.sidebarFg = Rgb(0xf5, 0xf5, 0xf5);
        t.sidebarPrimary = Rgb(0xf5, 0xf5, 0xf5);
        t.sidebarPrimaryFg = Rgb(0x0a, 0x0a, 0x0a);
        t.sidebarAccent = Rgb(0x26, 0x26, 0x26);
        t.sidebarAccentFg = Rgb(0xf5, 0xf5, 0xf5);
        t.sidebarBorder = Rgb(0x26, 0x26, 0x26);
        t.popover = Rgb(0x0a, 0x0a, 0x0a);
        t.popoverFg = Rgb(0xfa, 0xfa, 0xfa);
        t.scrollbarThumb = Rgba8(0x52, 0x52, 0x52, 0xe6);
        t.scrollbarThumbHover = Rgb(0x52, 0x52, 0x52);
        t.scrollbarBg = Rgba8(0x17, 0x17, 0x17, 0x00);
        t.info = Rgb(0x22, 0xd3, 0xee);
        t.infoFg = Rgb(0x08, 0x91, 0xb2);
        t.success = Rgb(0x4a, 0xde, 0x80);
        t.successFg = Rgb(0x16, 0xa3, 0x4a);
        t.warning = Rgb(0xfa, 0xcc, 0x15);
        t.warningFg = Rgb(0xca, 0x8a, 0x04);
        t.skeleton = Rgb(0x17, 0x17, 0x17);
        t.overlay = Rgba8(0, 0, 0, 0x33);
        t.groupBox = Rgb(0x0a, 0x0a, 0x0a);
        t.groupBoxFg = Rgb(0xfa, 0xfa, 0xfa);
        // description_list.label.background: the window background with the
        // border at 20% over it, which is what schema.rs falls back to. The
        // key default-theme.json spells is not the one the schema reads.
        t.descListLabel = Rgb(0x0f, 0x0f, 0x0f);
        // description_list.label.foreground falls back to muted_foreground,
        // not to the foreground: a label reads as a caption beside its value.
        t.descListLabelFg = Rgb(0xa3, 0xa3, 0xa3);
        t.radius = 6;
        t.radiusLg = 8;
        // The three that only exist so a theme can spell them as gradients,
        // on the fallbacks schema.rs gives them.
        t.statusBar = t.titleBar;
        t.switchThumb = t.background;
        t.sliderThumb = t.background;
        ThemeFillDerived(&t, true);
        ThemeTokensReset(&t);
        init = true;
    }
    return t;
}

const Theme& ThemeDefaultLight() {
    static Theme t;
    static bool init = false;
    if (!init) {
        t.background = Rgb(0xff, 0xff, 0xff);
        t.foreground = Rgb(0x0a, 0x0a, 0x0a);
        t.border = Rgb(0xe5, 0xe5, 0xe5);
        t.mutedFg = Rgb(0x73, 0x73, 0x73);
        t.inputBorder = Rgb(0xe5, 0xe5, 0xe5);
        t.inputBg = Rgb(0xff, 0xff, 0xff);
        t.ring = Rgb(0xa3, 0xa3, 0xa3);
        t.caret = Rgb(0x0a, 0x0a, 0x0a);
        t.selection = Rgba8(0x55, 0xa0, 0xfc, 0x4c);
        t.dragBorder = Rgb(0x3b, 0x82, 0xf6);
        t.titleBar = Rgb(0xf8, 0xf8, 0xf8);
        t.titleBarBorder = Rgb(0xe5, 0xe5, 0xe5);
        t.statusBarBorder = Rgb(0xe5, 0xe5, 0xe5);
        t.tabBar = Rgb(0xf5, 0xf5, 0xf5);
        t.tabActiveBg = Rgb(0xff, 0xff, 0xff);
        t.tabActiveFg = Rgb(0x17, 0x17, 0x17);
        t.tabFg = Rgb(0x40, 0x40, 0x40);
        t.tableBg = Rgb(0xff, 0xff, 0xff);
        t.tableHead = Rgb(0xfa, 0xfa, 0xfa);
        t.tableHeadFg = Rgb(0x73, 0x73, 0x73);
        t.tableFoot = Rgb(0xfa, 0xfa, 0xfa);
        t.tableFootFg = t.mutedFg;
        t.tableRowBorder = Rgba8(0xe5, 0xe5, 0xe5, 0xb3);
        t.tableEven = Rgb(0xfa, 0xfa, 0xfa);
        // default-theme.json light: the same blue for the list and the table.
        t.listActive = Rgba8(0xbf, 0xdb, 0xfe, 0x33);
        t.listActiveBorder = Rgb(0x60, 0xa5, 0xfa);
        t.tableActive = t.listActive;
        t.tableActiveBorder = t.listActiveBorder;
        t.progress = Rgb(0x17, 0x17, 0x17);
        t.red = Rgb(0xdc, 0x26, 0x26);
        t.green = Rgb(0x16, 0xa3, 0x4a);
        t.blue = Rgb(0x25, 0x63, 0xeb);
        t.yellow = Rgb(0xca, 0x8a, 0x04);
        t.cyan = Rgb(0x08, 0x91, 0xb2);
        t.magenta = Rgb(0x93, 0x33, 0xea);
        t.redLight = Rgb(0xf8, 0x71, 0x71);
        t.greenLight = Rgb(0x4a, 0xde, 0x80);
        t.blueLight = Rgb(0x60, 0xa5, 0xfa);
        t.yellowLight = Rgb(0xfa, 0xcc, 0x15);
        t.cyanLight = Rgb(0x22, 0xd3, 0xee);
        t.magentaLight = Rgb(0xc0, 0x84, 0xfc);
        t.chart1 = Rgb(0x93, 0xc5, 0xfd);
        t.chart2 = Rgb(0x3b, 0x82, 0xf6);
        t.chart3 = Rgb(0x25, 0x63, 0xeb);
        t.chart4 = Rgb(0x1d, 0x4e, 0xd8);
        t.chart5 = Rgb(0x1e, 0x40, 0xaf);
        t.chartBullish = Rgb(0x16, 0xa3, 0x4a);
        t.chartBearish = Rgb(0xdc, 0x26, 0x26);
        t.danger = Rgb(0xef, 0x44, 0x44);
        t.dangerFg = Rgb(0xfa, 0xfa, 0xfa);
        t.secondaryHover = Rgb(0xe5, 0xe5, 0xe5);
        t.secondaryActive = Rgb(0xd4, 0xd4, 0xd4);
        t.secondaryFg = Rgb(0x17, 0x17, 0x17);
        t.secondary = Rgb(0xe5, 0xe5, 0xe5);
        t.muted = Rgb(0xf5, 0xf5, 0xf5);
        t.accent = Rgb(0xf5, 0xf5, 0xf5);
        t.accentFg = Rgb(0x17, 0x17, 0x17);
        t.primary = Rgb(0x17, 0x17, 0x17);
        t.primaryFg = Rgb(0xfa, 0xfa, 0xfa);
        t.primaryHover = Rgb(0x26, 0x26, 0x26);
        t.primaryActive = Rgb(0x0a, 0x0a, 0x0a);
        t.sidebar = Rgb(0xfa, 0xfa, 0xfa);
        t.sidebarFg = Rgb(0x17, 0x17, 0x17);
        t.sidebarPrimary = Rgb(0x17, 0x17, 0x17);
        t.sidebarPrimaryFg = Rgb(0xfa, 0xfa, 0xfa);
        t.sidebarAccent = Rgb(0xe5, 0xe5, 0xe5);
        t.sidebarAccentFg = Rgb(0x17, 0x17, 0x17);
        t.sidebarBorder = Rgb(0xe5, 0xe5, 0xe5);
        t.popover = Rgb(0xff, 0xff, 0xff);
        t.popoverFg = Rgb(0x0a, 0x0a, 0x0a);
        t.scrollbarThumb = Rgba8(0xa3, 0xa3, 0xa3, 0xe6);
        t.scrollbarThumbHover = Rgb(0xa3, 0xa3, 0xa3);
        t.scrollbarBg = Rgba8(0xfa, 0xfa, 0xfa, 0x00);
        t.info = Rgb(0x06, 0xb6, 0xd4);
        t.infoFg = Rgb(0xfa, 0xfa, 0xfa);
        t.success = Rgb(0x22, 0xc5, 0x5e);
        t.successFg = Rgb(0xfa, 0xfa, 0xfa);
        t.warning = Rgb(0xea, 0xb3, 0x08);
        t.warningFg = Rgb(0xfa, 0xfa, 0xfa);
        t.skeleton = Rgb(0xf5, 0xf5, 0xf5);
        t.overlay = Rgba8(0, 0, 0, 0x0d);
        t.groupBox = Rgb(0xf5, 0xf5, 0xf5);
        t.groupBoxFg = Rgb(0x17, 0x17, 0x17);
        t.descListLabel = Rgb(0xf9, 0xf9, 0xf9);
        t.descListLabelFg = Rgb(0x73, 0x73, 0x73);
        t.radius = 6;
        t.radiusLg = 8;
        // The three that only exist so a theme can spell them as gradients,
        // on the fallbacks schema.rs gives them.
        t.statusBar = t.titleBar;
        t.switchThumb = t.background;
        t.sliderThumb = t.background;
        ThemeFillDerived(&t, false);
        ThemeTokensReset(&t);
        init = true;
    }
    return t;
}

// The pair in force. The two above are what a theme file is resolved
// against and never change; these are what everything paints from, and what
// ThemeInstall replaces when a theme is applied.
static Theme gActiveTheme[2];
static bool gActiveThemeInit = false;

static void ActiveThemeInit() {
    if (gActiveThemeInit) {
        return;
    }
    gActiveThemeInit = true;
    gActiveTheme[(int)ThemeMode::Light] = ThemeDefaultLight();
    gActiveTheme[(int)ThemeMode::Dark] = ThemeDefaultDark();
}

const Theme& ThemeLight() {
    ActiveThemeInit();
    return gActiveTheme[(int)ThemeMode::Light];
}

const Theme& ThemeDark() {
    ActiveThemeInit();
    return gActiveTheme[(int)ThemeMode::Dark];
}

void ThemeInstall(ThemeMode mode, const Theme& t) {
    ActiveThemeInit();
    gActiveTheme[(int)mode] = t;
}

static ThemeMode gThemeMode = ThemeMode::Light;
// theme/mod.rs: radius 6, radius_lg 8, font_size 16.
static const float kDefaultFontSize = 16.f;
static float gFontSize = kDefaultFontSize;
static ScrollbarMode gScrollbarMode = ScrollbarMode::Always;
// theme/mod.rs: `focus_ring: true`.
static bool gFocusRing = true;

void ThemeSetRadius(float radius) {
    // Both palettes, since a theme here is a static rather than one window's
    // Global and the mode can be switched under it.
    const Theme* both[2] = {&ThemeLight(), &ThemeDark()};
    for (int i = 0; i < 2; i++) {
        Theme* t = const_cast<Theme*>(both[i]);
        t->radius = radius;
        t->radiusLg = radius > 0 ? radius + 2 : 0;
    }
}

float ThemeFontSize() {
    return gFontSize;
}

void ThemeSetFontSize(float px) {
    gFontSize = px > 0 ? px : kDefaultFontSize;
}

// What an explicit `Font(12)` is multiplied by. Rust says its sizes in rems,
// so they all follow `Theme::font_size`; these are in DIPs, so the base is
// what they are measured against.
static float ThemeFontScale() {
    return gFontSize / kDefaultFontSize;
}

bool ThemeFocusRing() {
    return gFocusRing;
}

void ThemeSetFocusRing(bool on) {
    gFocusRing = on;
}

ScrollbarMode ScrollbarModeNow() {
    return gScrollbarMode;
}

void ScrollbarModeSet(ScrollbarMode m) {
    gScrollbarMode = m;
}

void ThemeSet(App* app, ThemeMode mode) {
    if (app) {
        app->themeMode = mode;
    }
    // Painting happens below Ctx, so it reads the mode from here.
    gThemeMode = mode;
}

ThemeMode ThemeGet() {
    return gThemeMode;
}

const Theme& ThemeNow() {
    return gThemeMode == ThemeMode::Dark ? ThemeDark() : ThemeLight();
}

// ─── element builders ─────────────────────────────────────────────────────

static El* NewEl(Arena* a, ElKind k) {
    El* e = ArenaNew<El>(a);
    e->arena = a;
    e->kind = k;
    return e;
}

El* Div(Arena* a) {
    return NewEl(a, ElKind::Div);
}

El* TextEl(Arena* a, Str s) {
    El* e = NewEl(a, ElKind::Text);
    e->text = s;
    return e;
}

El* IconEl(Arena* a, IconName name) {
    return IconEl(a, name, 16.f);
}

El* IconEl(Arena* a, IconName name, float size) {
    El* e = NewEl(a, ElKind::Icon);
    e->icon = name;
    e->iconPath = IconNamePath(name);
    e->style.width = size;
    e->style.height = size;
    e->style.flexShrink = 0;
    return e;
}

El* ImageEl(Arena* a, Str src, Str alt) {
    El* e = NewEl(a, ElKind::Image);
    e->imgSrc = src;
    e->text = alt;
    e->style.flexShrink = 0;
    return e;
}

El* ButtonEl(Arena* a, int clickId, Str label, BtnKind kind) {
    return ButtonSmall(a, clickId, label, kind, false);
}

El* ButtonSmall(Arena* a, int clickId, Str label, BtnKind kind, bool selected) {
    const Theme& th = ThemeNow();
    El* b = Div(a)
                ->ItemsCenter()
                ->JustifyCenter()
                ->Radius(th.radius)
                ->Click(clickId)
                ->FocusId(clickId);
    if (kind == BtnKind::Primary) {
        b->PadX(16)
            ->PadY(8)
            ->Bg(th.tokens.primary)
            ->HoverBg(RgbaMix(th.primary, th.foreground, 0.85f));
        b->Child(TextEl(a, label)->Font(14)->Fg(th.primaryFg));
    } else if (kind == BtnKind::Outline) {
        b->PadX(16)->PadY(8)->Border(1, th.border)->HoverBg(th.tokens.muted);
        b->Child(TextEl(a, label)->Font(14)->Fg(th.foreground));
    } else {
        b->PadX(12)
            ->PadY(6)
            ->Bg(selected ? th.secondaryActive : th.secondary)
            ->HoverBg(th.secondaryHover);
        b->Child(
            TextEl(a, label)->Font(selected ? 13.f : 14.f)->Fg(th.secondaryFg));
    }
    return b;
}

El* ProgressEl(Arena* a, float value01to100, float barW, float barH) {
    El* e = NewEl(a, ElKind::Progress);
    e->progress = value01to100;
    if (e->progress < 0) {
        e->progress = 0;
    }
    if (e->progress > 100) {
        e->progress = 100;
    }
    e->style.width = barW;
    e->style.height = barH;
    e->style.flexShrink = 0;
    e->style.radius = barH * 0.5f;
    return e;
}

El* ChartEl(Arena* a, const float* ys, int n, Rgba stroke, Rgba fillTop,
            Rgba fillBot, int tickMargin) {
    El* e = NewEl(a, ElKind::Chart);
    e->chart.ys = ys;
    e->chart.n = n;
    e->chart.stroke = stroke;
    e->chart.fillTop = fillTop;
    e->chart.fillBot = fillBot;
    e->chart.tickMargin = tickMargin > 0 ? tickMargin : 15;
    e->style.flexGrow = 1;
    e->style.height = kFill;
    e->style.minH = 80;
    return e;
}

// The flex model is on for a box that says so, and for one that sets an
// alignment, a justification or a gap. Rust does not do the second half —
// `div().items_center()` there is a block container with a property that does
// nothing — but Rust callers write `h_flex()` when they mean a row, and this
// tree's callers wrote the alignment because until now every box was a flex
// container. Reading the intent from the alignment keeps those calls meaning
// what the person who wrote them saw, and leaves a bare `Div()` as the block
// container `div()` is.
El* El::Flex() {
    style.display = Display::Flex;
    return this;
}
El* El::FlexRow() {
    style.display = Display::Flex;
    style.dir = FlexDir::Row;
    return this;
}
El* El::FlexCol() {
    style.display = Display::Flex;
    style.dir = FlexDir::Col;
    return this;
}
El* El::FlexRowReverse() {
    style.display = Display::Flex;
    style.dir = FlexDir::RowReverse;
    return this;
}
El* El::FlexColReverse() {
    style.display = Display::Flex;
    style.dir = FlexDir::ColReverse;
    return this;
}
El* El::FlexWrap() {
    style.display = Display::Flex;
    style.flexWrap = true;
    return this;
}
El* El::Grow(float g) {
    style.flexGrow = g;
    return this;
}
El* El::Shrink0() {
    style.flexShrink = 0;
    return this;
}
El* El::Flex1() {
    style.flexGrow = 1;
    style.flexShrink = 1;
    style.flexBasis = 0;
    return this;
}
El* El::FlexNone() {
    style.flexGrow = 0;
    style.flexShrink = 0;
    style.flexBasis = kAuto;
    return this;
}
El* El::Basis(float v) {
    style.flexBasis = v;
    return this;
}
El* El::BasisFrac(float f) {
    style.flexBasisFrac = f;
    return this;
}
El* El::Shrink(float f) {
    style.flexShrink = f;
    return this;
}
El* El::W(float v) {
    style.width = v;
    return this;
}
El* El::WFrac(float f) {
    style.widthFrac = f;
    return this;
}
El* El::H(float v) {
    style.height = v;
    return this;
}
El* El::SizeFull() {
    style.width = kFill;
    style.height = kFill;
    style.flexGrow = 1;
    return this;
}
El* El::MinH(float v) {
    style.minH = v;
    return this;
}
El* El::MinW(float v) {
    style.minW = v;
    return this;
}
El* El::MaxW(float v) {
    style.maxW = v;
    return this;
}
El* El::MaxH(float v) {
    style.maxH = v;
    return this;
}
El* El::Gap(float v) {
    style.display = Display::Flex;
    style.gapX = v;
    style.gapY = v;
    return this;
}
El* El::GapX(float v) {
    style.display = Display::Flex;
    style.gapX = v;
    return this;
}
El* El::GapY(float v) {
    style.display = Display::Flex;
    style.gapY = v;
    return this;
}
El* El::Pad(float v) {
    style.pad = {v, v, v, v};
    return this;
}
El* El::PadX(float v) {
    style.pad.left = style.pad.right = v;
    return this;
}
El* El::PadY(float v) {
    style.pad.top = style.pad.bottom = v;
    return this;
}
El* El::PadL(float v) {
    style.pad.left = v;
    return this;
}
El* El::PadR(float v) {
    style.pad.right = v;
    return this;
}
El* El::PadT(float v) {
    style.pad.top = v;
    return this;
}
El* El::PadB(float v) {
    style.pad.bottom = v;
    return this;
}
El* El::ItemsCenter() {
    style.display = Display::Flex;
    style.align = Align::Center;
    return this;
}
El* El::ItemsStart() {
    style.display = Display::Flex;
    style.align = Align::Start;
    return this;
}
El* El::ItemsEnd() {
    style.display = Display::Flex;
    style.align = Align::End;
    return this;
}
El* El::ItemsStretch() {
    style.display = Display::Flex;
    style.align = Align::Stretch;
    return this;
}
El* El::JustifyBetween() {
    style.display = Display::Flex;
    style.justify = Justify::SpaceBetween;
    return this;
}
El* El::JustifyAround() {
    style.display = Display::Flex;
    style.justify = Justify::SpaceAround;
    return this;
}
El* El::JustifyCenter() {
    style.display = Display::Flex;
    style.justify = Justify::Center;
    return this;
}
El* El::JustifyEnd() {
    style.display = Display::Flex;
    style.justify = Justify::End;
    return this;
}
El* El::JustifyStart() {
    style.display = Display::Flex;
    style.justify = Justify::Start;
    return this;
}
El* El::Bg(Background c) {
    style.bg = c;
    style.hasBg = true;
    return this;
}
El* El::Border(float width, Rgba c) {
    style.border = width;
    style.borderColor = c;
    return this;
}
El* El::BorderT(float width, Rgba c) {
    style.borderT = width;
    style.borderColor = c;
    return this;
}
El* El::BorderB(float width, Rgba c) {
    style.borderB = width;
    style.borderColor = c;
    return this;
}
El* El::BorderL(float width, Rgba c) {
    style.borderL = width;
    style.borderColor = c;
    return this;
}
El* El::BorderR(float width, Rgba c) {
    style.borderR = width;
    style.borderColor = c;
    return this;
}
El* El::DashArray(float on, float off) {
    style.dashOn = on;
    style.dashOff = off;
    return this;
}
El* El::Radius(float r) {
    style.radius = r;
    return this;
}
El* El::Corners(float tl, float tr, float br, float bl) {
    style.corners = {tl, tr, br, bl};
    style.hasCorners = true;
    // `radius` stays what the uniform case reads, so anything that only knows
    // about one number — the focus ring's own corners — still lands near it.
    style.radius = tl > tr ? tl : tr;
    return this;
}
El* El::Fg(Rgba c) {
    style.color = c;
    style.hasColor = true;
    return this;
}
El* El::Font(float px) {
    style.fontSize = px;
    return this;
}
El* El::LineHeight(float mult) {
    style.lineHeight = mult;
    return this;
}
El* El::Truncate() {
    style.truncate = true;
    return this;
}
El* El::ClipY() {
    style.overflowY = Overflow::Hidden;
    return this;
}
El* El::ScrollY(float off) {
    style.overflowY = Overflow::Scroll;
    scrollY = off;
    return this;
}
El* El::ScrollX(float off) {
    style.overflowX = Overflow::Scroll;
    scrollX = off;
    return this;
}
El* El::ClipX() {
    style.overflowX = Overflow::Hidden;
    return this;
}
// The bar an element shows: its own when it named one, the theme's default
// otherwise.
static ScrollbarMode ElScrollMode(const El* e) {
    return e->scrollModeSet ? e->scrollMode : ScrollbarModeNow();
}

// The thumb's colour: `thumb_hover` under the pointer or in a drag, `thumb`
// otherwise, faded by however far through the Scrolling fade the bar is.
static Background ScrollbarThumbBg(const Theme& th, bool hot, float alpha) {
    Background c =
        hot ? th.tokens.scrollbarThumbHover : th.tokens.scrollbarThumb;
    return alpha >= 1.f ? c : BackgroundOpacity(c, alpha);
}

// The track behind it — `scrollbar.background`, which both default themes
// leave transparent.
static Background ScrollbarBarBg(const Theme& th, float alpha) {
    Background c = Background(th.scrollbarBg);
    return alpha >= 1.f ? c : BackgroundOpacity(c, alpha);
}

// clamp_thumb_radius: the theme's radius, never more than half the thumb.
static float ThumbRadius(const Theme& th, float thumbW) {
    float r = th.radius;
    return r > thumbW * 0.5f ? thumbW * 0.5f : r;
}

// ScrollbarMode::Scrolling's clock. Rust keeps `last_scroll_offset` and
// `last_scroll_time` in the scrollbar element's own keyed state; the tree here
// is rebuilt every frame, so the pair lives beside the tree and is found again
// by `El::ScrollId`. One entry per scroll area that has ever moved — a
// gallery has a handful — and they are dropped with the app.
struct ScrollFade {
    int id = 0;
    float y = 0;
    float x = 0;
    // TimeNow() when the offset last changed, or when the pointer last held
    // the bar up.
    double at = 0;
};

static Vec<ScrollFade> gScrollFades;

static ScrollFade* ScrollFadeFor(int id, float y, float x) {
    for (int i = 0; i < gScrollFades.len; i++) {
        if (gScrollFades[i].id == id) {
            return &gScrollFades[i];
        }
    }
    ScrollFade f;
    f.id = id;
    f.y = y;
    f.x = x;
    // A bar the frame has never seen starts out faded: opening a page does not
    // flash every scrollbar on it, which is what Rust gets from having no
    // last_scroll_time until something scrolls.
    f.at = -(double)kScrollbarFadeDuration;
    gScrollFades.Append(f);
    return &gScrollFades[gScrollFades.len - 1];
}

void ScrollFadeClear() {
    gScrollFades.Reset();
}

// ─── the inspector's live style overrides ────────────────────────────────

struct StyleOverride {
    int clickId = 0;
    uint32_t fields = 0;
    Style style = {};
};

static Vec<StyleOverride> gStyleOverrides;

void StyleOverrideSet(int clickId, uint32_t fields, const Style& style) {
    if (clickId == 0) {
        return;
    }
    if (fields == 0) {
        StyleOverrideClear(clickId);
        return;
    }
    for (int i = 0; i < gStyleOverrides.len; i++) {
        if (gStyleOverrides[i].clickId == clickId) {
            gStyleOverrides[i].fields = fields;
            gStyleOverrides[i].style = style;
            return;
        }
    }
    StyleOverride o;
    o.clickId = clickId;
    o.fields = fields;
    o.style = style;
    gStyleOverrides.Append(o);
}

void StyleOverrideClear(int clickId) {
    for (int i = 0; i < gStyleOverrides.len; i++) {
        if (gStyleOverrides[i].clickId == clickId) {
            for (int j = i + 1; j < gStyleOverrides.len; j++) {
                gStyleOverrides[j - 1] = gStyleOverrides[j];
            }
            gStyleOverrides.len--;
            return;
        }
    }
}

void StyleOverrideClearAll() {
    gStyleOverrides.Reset();
}

void StyleApplyFields(Style* into, const Style& over, uint32_t fields) {
    if (!into || fields == 0) {
        return;
    }
    if (fields & StyleFieldBg) {
        into->bg = over.bg;
        into->hasBg = true;
    }
    if (fields & StyleFieldColor) {
        into->color = over.color;
        into->hasColor = true;
    }
    if (fields & StyleFieldBorderColor) {
        into->borderColor = over.borderColor;
    }
    if (fields & StyleFieldPad) {
        into->pad = over.pad;
    }
    if (fields & StyleFieldGap) {
        into->gapX = over.gapX;
        into->gapY = over.gapY;
    }
    if (fields & StyleFieldRadius) {
        into->radius = over.radius;
    }
    if (fields & StyleFieldBorder) {
        into->border = over.border;
    }
    if (fields & StyleFieldFontSize) {
        into->fontSize = over.fontSize;
    }
    if (fields & StyleFieldWidth) {
        into->width = over.width;
    }
    if (fields & StyleFieldHeight) {
        into->height = over.height;
    }
    if (fields & StyleFieldOpacity) {
        into->opacity = over.opacity;
    }
    if (fields & StyleFieldHoverBg) {
        into->hoverBg = over.hoverBg;
        into->hasHoverBg = true;
    }
    if (fields & StyleFieldHoverFg) {
        into->hoverFg = over.hoverFg;
        into->hasHoverFg = true;
    }
}

void StyleOverrideApply(El* e) {
    // Nothing picked, nothing edited: the common case costs one compare.
    if (gStyleOverrides.len == 0 || !e || e->clickId == 0) {
        return;
    }
    for (int i = 0; i < gStyleOverrides.len; i++) {
        const StyleOverride& o = gStyleOverrides[i];
        if (o.clickId != e->clickId) {
            continue;
        }
        StyleApplyFields(&e->style, o.style, o.fields);
        return;
    }
}

// How opaque a Scrolling bar is right now, and whether the window has to come
// back for the rest of the fade. `held` is the pointer resting on the bar,
// which Rust answers by stamping the time again.
static float ScrollFadeOpacity(int id, float y, float x, bool held,
                               bool* wantsFrame) {
    ScrollFade* f = ScrollFadeFor(id, y, x);
    double now = TimeNow();
    if (f->y != y || f->x != x || held) {
        f->y = y;
        f->x = x;
        f->at = now;
    }
    float elapsed = (float)(now - f->at);
    if (elapsed < kScrollbarFadeDelay) {
        *wantsFrame = true;
        return 1.f;
    }
    if (elapsed >= kScrollbarFadeDuration) {
        return 0.f;
    }
    *wantsFrame = true;
    // 1 - t^10 over the last second, which is Rust's curve.
    float t = elapsed - kScrollbarFadeDelay;
    float t2 = t * t;
    float t4 = t2 * t2;
    float o = 1.f - t4 * t4 * t2;
    return o < 0 ? 0 : o;
}

El* El::Rotate(float turns) {
    style.rotate = turns;
    return this;
}

El* El::HideScrollbar() {
    noScrollbar = true;
    return this;
}

El* El::HideScrollbarX() {
    noScrollbarX = true;
    return this;
}

El* El::HideScrollbarY() {
    noScrollbarY = true;
    return this;
}

El* El::Opacity(float f) {
    style.opacity = f < 0 ? 0 : (f > 1 ? 1 : f);
    return this;
}

El* El::ScrollMode(ScrollbarMode m) {
    scrollModeSet = true;
    scrollMode = m;
    return this;
}
El* El::ScrollId(int v) {
    scrollId = v;
    return this;
}
El* El::Click(int v) {
    clickId = v;
    return this;
}
El* El::OnClick(Func0 fn) {
    onClick = fn;
    return this;
}
El* El::OnClick(Listener l) {
    listener = l;
    return this;
}
El* El::OnScroll(Listener l) {
    onScroll = l;
    return this;
}
El* El::OnHover(Listener l) {
    onHover = l;
    return this;
}
El* El::OnMouseDown(Listener l, DispatchPhase phase) {
    onMouseDown = l;
    mouseDownPhase = phase;
    return this;
}
El* El::OnMouseUp(Listener l, DispatchPhase phase) {
    onMouseUp = l;
    mouseUpPhase = phase;
    return this;
}
El* El::OnDragMove(Listener l) {
    onDragMove = l;
    return this;
}
El* El::OnDrag(Str dragKind, int ix, void* data) {
    drag.kind = dragKind;
    drag.ix = ix;
    drag.data = data;
    return this;
}
El* El::OnMouseUpOut(Listener l) {
    onMouseUpOut = l;
    return this;
}
El* El::OnDrop(Str acceptKind, Listener l) {
    dropKind = acceptKind;
    onDrop = l;
    return this;
}
El* El::Refine(const Style& s, uint32_t fields) {
    if (fields == 0) {
        return this;
    }
    // Two refinements on one element merge, the way StyleRefinement::refine
    // does: the second names what it names and leaves the rest.
    StyleApplyFields(&refine, s, fields);
    refineSet |= fields;
    return this;
}

El* El::BoundsOut(gpui::Bounds* out) {
    boundsOut = out;
    return this;
}
El* El::Cursor(CursorKind c) {
    cursor = c;
    return this;
}
El* El::BindSlider(SliderState* s, Axis axis) {
    slider = s;
    sliderAxis = axis;
    return this;
}
El* El::BindSliderBounds(SliderState* s) {
    sliderBounds = s;
    return this;
}
El* El::BindInput(InputState* s) {
    input = s;
    // InputState::key_context: every binding state.rs installs is scoped to
    // it, so a field's own chords only resolve while a field has the
    // keyboard. Declared here rather than by each caller, since an element
    // bound to an InputState is the field.
    if (s) {
        InputInitKeys();
        KeyContext(InputContext());
    }
    return this;
}
// InputElement paints the selection as a quad under the run and the caret as
// one on top of it. Both are measured against the shaped line, so a caret
// appearing and disappearing cannot shift the glyphs beside it.
El* El::SelRange(int lo, int hi, Rgba color) {
    selLo = lo;
    selHi = hi;
    selColor = color;
    return this;
}

El* El::CaretOut(float* outX, float* outY) {
    caretOutX = outX;
    caretOutY = outY;
    return this;
}
El* El::RangeOut(int lo, int hi, gpui::Bounds* out) {
    rangeOutLo = lo;
    rangeOutHi = hi;
    rangeOut = out;
    return this;
}
El* El::Washes(const TextSpan* runs, int n) {
    washes = runs;
    nWashes = n;
    return this;
}
El* El::Underlines(const TextSpan* runs, int n) {
    underlines = runs;
    nUnderlines = n;
    return this;
}
El* El::Spans(const TextSpan* runs, int n) {
    spans = runs;
    nSpans = n;
    return this;
}
int Utf8OffsetToUtf16(Str s, int u8) {
    if (u8 > s.len) {
        u8 = s.len;
    }
    int u16 = 0;
    int i = 0;
    while (i < u8) {
        unsigned char c = (unsigned char)s.s[i];
        int len = c < 0x80 ? 1 : (c < 0xE0 ? 2 : (c < 0xF0 ? 3 : 4));
        // Everything outside the basic plane is a surrogate pair over there.
        u16 += len == 4 ? 2 : 1;
        i += len;
    }
    return u16;
}

int Utf16OffsetToUtf8(Str s, int u16) {
    int at = 0;
    int i = 0;
    while (i < s.len && at < u16) {
        unsigned char c = (unsigned char)s.s[i];
        int len = c < 0x80 ? 1 : (c < 0xE0 ? 2 : (c < 0xF0 ? 3 : 4));
        at += len == 4 ? 2 : 1;
        i += len;
    }
    return i;
}

El* El::MarkRange(int lo, int hi) {
    markLo = lo;
    markHi = hi;
    return this;
}
El* El::Caret(int off, Rgba color, float width) {
    caretOff = off;
    caretColor = color;
    caretW = width;
    return this;
}

bool ClickFromRelease(bool pending, int pressedId, MouseButton pressedButton,
                      bool dragged, int upId, MouseButton upButton) {
    if (!pending) {
        return false;
    }
    if (dragged) {
        return false;
    }
    if (upButton != pressedButton) {
        return false;
    }
    return upId == pressedId;
}

bool ClickFromKeyRelease(bool pending, int pendingGen, int focusGen, int key,
                         bool modified) {
    if (!pending || modified) {
        return false;
    }
    if (key != KeyReturn && key != KeySpace) {
        return false;
    }
    // The focus moved between the two halves, so the element that would take
    // the click is not the one the key went down on.
    return pendingGen == focusGen;
}

int HashClickId(Str s) {
    uint32_t h = 2166136261u;
    if (s.s) {
        for (int i = 0; i < s.len; i++) {
            h ^= (uint8_t)s.s[i];
            h *= 16777619u;
        }
    }
    int id = (int)(h & 0x3fffffff);
    if (id < 1000) {
        id += 1000;
    }
    return id;
}
El* El::Bold() {
    style.fontBold = true;
    return this;
}
El* El::Semibold() {
    style.fontSemibold = true;
    return this;
}
El* El::Medium() {
    style.fontMedium = true;
    return this;
}
El* El::Mono() {
    style.fontMono = true;
    return this;
}
El* El::Underline() {
    style.underline = true;
    return this;
}
El* El::Strikethrough() {
    style.strike = true;
    return this;
}
El* El::Italic() {
    style.italic = true;
    return this;
}
El* El::Selectable() {
    selectable = true;
    return this;
}
El* El::Wrap() {
    style.wrap = true;
    return this;
}
El* El::Dashed() {
    style.borderDashed = true;
    return this;
}
El* El::Absolute() {
    style.absolute = true;
    return this;
}
El* El::Fixed() {
    style.absolute = true;
    style.fixed = true;
    return this;
}
El* El::Deferred() {
    style.deferred = true;
    return this;
}
El* El::AnchorBelow(float gap) {
    style.absolute = true;
    style.anchorBelow = true;
    style.anchorGap = gap;
    return this;
}
El* El::AnchorAbove(float gap) {
    style.absolute = true;
    style.anchorAbove = true;
    style.anchorGap = gap;
    return this;
}
El* El::AnchorCenterX() {
    style.absolute = true;
    style.anchorCenterX = true;
    return this;
}
El* El::Top(float v) {
    style.absTop = v;
    return this;
}
El* El::LeftRel(float frac) {
    style.absLeftRel = frac;
    return this;
}
El* El::RightRel(float frac) {
    style.absRightRel = frac;
    return this;
}
El* El::Left(float v) {
    style.absLeft = v;
    return this;
}
El* El::Bottom(float v) {
    style.absBottom = v;
    return this;
}
El* El::Right(float v) {
    style.absRight = v;
    return this;
}
El* El::HoverBg(Background c) {
    style.hoverBg = c;
    style.hasHoverBg = true;
    return this;
}
El* El::HoverFg(Rgba c) {
    style.hoverFg = c;
    style.hasHoverFg = true;
    return this;
}

El* El::FocusOnPress(bool v) {
    style.focusOnPress = v;
    return this;
}

El* El::Group() {
    style.group = true;
    return this;
}

El* El::GroupHoverVisible() {
    style.groupHoverVisible = true;
    return this;
}
El* El::FocusId(int v) {
    style.focusId = v;
    return this;
}
El* El::KeyContext(Str name) {
    style.keyContext = KeyContextOf(name);
    return this;
}
El* El::OnKeyDown(Listener fn) {
    return OnAction(ActionOf(StrL("gpui::KeyDown")), fn);
}

El* El::OnClickAction(uint32_t action, intptr_t arg) {
    clickAction = action;
    clickActionArg = arg;
    return this;
}

El* El::OnAction(uint32_t action, Listener fn) {
    if (!action || !fn.IsValid()) {
        return this;
    }
    // Newest first, which reads the same way as adding a handler to a builder
    // and having it seen before the ones already there.
    ActionSlot* slot = ArenaNew<ActionSlot>(arena);
    slot->action = action;
    slot->fn = fn;
    slot->next = actions;
    actions = slot;
    return this;
}
El* El::TabIndex(int v) {
    style.tabIndex = v;
    return this;
}
El* El::TabStop(bool v) {
    style.tabStop = v;
    return this;
}
El* El::FocusRing(bool v) {
    style.focusRing = v;
    return this;
}
El* El::TrapId(int v) {
    style.trapId = v;
    return this;
}
El* El::Tip(Str s) {
    style.tooltip = s;
    return this;
}
El* El::Id(Str s) {
    id = s;
    return this;
}
El* El::Child(El* c) {
    if (!c) {
        return this;
    }
    c->next = nullptr;
    if (last) {
        last->next = c;
    } else {
        first = c;
    }
    last = c;
    return this;
}

// ─── measure / layout ─────────────────────────────────────────────────────

float PxToDip(PaintCtx* ctx, int px) {
    return px * 96.f / (ctx->dpi > 0 ? ctx->dpi : 96.f);
}
int DipToPx(PaintCtx* ctx, float dip) {
    return (int)(dip * (ctx->dpi > 0 ? ctx->dpi : 96.f) / 96.f + 0.5f);
}

// Key wrap width: 0 = unconstrained. Round to 1 DIP so tiny parent-size
// jitter from extra layout passes still hits.
static float MeasKeyMaxW(float maxW, bool wrap) {
    if (!wrap || maxW <= 0) {
        return 0;
    }
    return floorf(maxW + 0.5f);
}

static float MeasKeyFont(float fontSize) {
    if (fontSize <= 0) {
        return 16.f;
    }
    return floorf(fontSize * 4.f + 0.5f) / 4.f;
}

static bool memeq(const void* s1, const void* s2, int n) {
    return 0 == memcmp(s1, s2, (size_t)n);
}

static uint32_t MurmurHash2(const void* key, int n) {
    if (n <= 0) {
        return 0;
    }
    const uint32_t m = 0x5bd1e995;
    const int r = 24;
    uint32_t h = 5381u ^ (uint32_t)n;
    const uint8_t* data = (const uint8_t*)key;
    while (n >= 4) {
        uint32_t k = *(uint32_t*)data;
        k *= m;
        k ^= k >> r;
        k *= m;
        h *= m;
        h ^= k;
        data += 4;
        n -= 4;
    }
    switch (n) {
        case 3:
            h ^= data[2] << 16;
            [[fallthrough]];
        case 2:
            h ^= data[1] << 8;
            [[fallthrough]];
        case 1:
            h ^= data[0];
            h *= m;
    }
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;
    return h;
}

static uint32_t MurmurHash2(Str s) {
    return MurmurHash2(s.s, s.len);
}

struct TextMeasSlot {
    char* text = nullptr;
    int len = 0;
    uint32_t hash = 0;
    float fontSize = 0;
    float maxW = 0;
    // Line height multiplier; 0 = the default phi box (see kLineHeight).
    float lineH = 0;
    float w = 0;
    float h = 0;
    uint32_t lastUsed = 0;
    TextLayout* layout = nullptr;
    uint8_t wrap = 0;
    uint8_t bold = 0;
    uint8_t occupied = 0;
};

static uint32_t TextMeasHash(Str s, float fontSize, float maxW, bool wrap,
                             uint8_t weight, float lineH) {
    uint32_t h = MurmurHash2(s);
    uint32_t fs = 0;
    uint32_t mw = 0;
    uint32_t lh = 0;
    memcpy(&fs, &fontSize, sizeof(fs));
    memcpy(&mw, &maxW, sizeof(mw));
    memcpy(&lh, &lineH, sizeof(lh));
    h ^= fs * 0x9e3779b9u;
    h ^= mw * 0x85ebca6bu;
    h ^= lh * 0xc2b2ae35u;
    if (wrap) {
        h ^= 0x165667b1u;
    }
    if (weight) {
        h ^= 0x27d4eb2fu * (uint32_t)weight;
    }
    return h;
}

static bool TextMeasKeyEq(const TextMeasSlot* sl, uint32_t hash, Str s,
                          float fontSize, float maxW, bool wrap, uint8_t weight,
                          float lineH) {
    if (!sl->occupied || sl->hash != hash || sl->len != s.len) {
        return false;
    }
    if (sl->fontSize != fontSize || sl->maxW != maxW || sl->lineH != lineH ||
        sl->wrap != (wrap ? 1 : 0) || sl->bold != weight) {
        return false;
    }
    return memeq(sl->text, s.s, s.len);
}

static uint8_t ElTextWeight(const El* e) {
    uint8_t w = kFontWeightNormal;
    if (e->style.fontBold) {
        w = kFontWeightBold;
    } else if (e->style.fontSemibold) {
        w = kFontWeightSemibold;
    } else if (e->style.fontMedium) {
        w = kFontWeightMedium;
    }
    if (e->style.fontMono) {
        w |= kFontMono;
    }
    if (e->style.underline) {
        w |= kFontUnderline;
    }
    if (e->style.strike) {
        w |= kFontStrike;
    }
    if (e->style.italic) {
        w |= kFontItalic;
    }
    return w;
}

static void TextMeasFreeSlot(TextMeasSlot* sl) {
    if (!sl) {
        return;
    }
    if (sl->text) {
        StrFree(Str{sl->text, sl->len});
        sl->text = nullptr;
    }
    if (sl->layout) {
        TextLayoutRelease(sl->layout);
        sl->layout = nullptr;
    }
    sl->occupied = 0;
    sl->len = 0;
}

static TextMeasSlot* TextMeasFind(TextMeasCache* c, Str s, float fontSize,
                                  float maxW, bool wrap, uint8_t weight,
                                  float lineH, uint32_t* outHash) {
    float keyFont = MeasKeyFont(fontSize);
    float keyMaxW = MeasKeyMaxW(maxW, wrap);
    uint32_t hash = TextMeasHash(s, keyFont, keyMaxW, wrap, weight, lineH);
    if (outHash) {
        *outHash = hash;
    }
    if (!c->slots || c->cap <= 0) {
        return nullptr;
    }
    int mask = c->cap - 1;
    int i = (int)(hash & (uint32_t)mask);
    for (int n = 0; n < c->cap; n++) {
        TextMeasSlot* sl = &((TextMeasSlot*)c->slots)[i];
        if (!sl->occupied) {
            return nullptr;
        }
        if (TextMeasKeyEq(sl, hash, s, keyFont, keyMaxW, wrap, weight, lineH)) {
            return sl;
        }
        i = (i + 1) & mask;
    }
    return nullptr;
}

static void TextMeasInsertMove(TextMeasCache* c, TextMeasSlot* src);

static void TextMeasGrow(TextMeasCache* c, int minCap) {
    int cap = c->cap > 0 ? c->cap : 256;
    while (cap < minCap) {
        cap *= 2;
    }
    TextMeasSlot* old = (TextMeasSlot*)c->slots;
    int oldCap = c->cap;
    TextMeasSlot* neu = AllocArray<TextMeasSlot>(cap);
    if (!neu) {
        return;
    }
    c->slots = neu;
    c->cap = cap;
    c->used = 0;
    if (old) {
        for (int i = 0; i < oldCap; i++) {
            if (old[i].occupied) {
                TextMeasInsertMove(c, &old[i]);
            }
        }
        Free(nullptr, old);
    }
}

static void TextMeasInsertMove(TextMeasCache* c, TextMeasSlot* src) {
    if (!c->slots || c->cap <= 0) {
        return;
    }
    int mask = c->cap - 1;
    int i = (int)(src->hash & (uint32_t)mask);
    for (int n = 0; n < c->cap; n++) {
        TextMeasSlot* sl = &((TextMeasSlot*)c->slots)[i];
        if (!sl->occupied) {
            *sl = *src;
            sl->occupied = 1;
            c->used++;
            src->text = nullptr;
            src->layout = nullptr;
            src->occupied = 0;
            return;
        }
        i = (i + 1) & mask;
    }
    TextMeasFreeSlot(src);
}

static TextMeasSlot* TextMeasInsert(PaintCtx* ctx, Str s, float fontSize,
                                    float maxW, bool wrap, uint8_t weight,
                                    float lineH, float w, float h,
                                    TextLayout* layout) {
    TextMeasCache* c = &ctx->textCache;
    float keyFont = MeasKeyFont(fontSize);
    float keyMaxW = MeasKeyMaxW(maxW, wrap);
    uint32_t hash = TextMeasHash(s, keyFont, keyMaxW, wrap, weight, lineH);
    if (c->cap == 0 || (c->used + 1) * 10 > c->cap * 6) {
        TextMeasGrow(c, c->cap > 0 ? c->cap * 2 : 256);
    }
    if (!c->slots || c->cap <= 0) {
        return nullptr;
    }
    int mask = c->cap - 1;
    int i = (int)(hash & (uint32_t)mask);
    TextMeasSlot* sl = nullptr;
    for (int n = 0; n < c->cap; n++) {
        TextMeasSlot* cand = &((TextMeasSlot*)c->slots)[i];
        if (!cand->occupied) {
            sl = cand;
            break;
        }
        if (TextMeasKeyEq(cand, hash, s, keyFont, keyMaxW, wrap, weight,
                          lineH)) {
            sl = cand;
            break;
        }
        i = (i + 1) & mask;
    }
    if (!sl) {
        return nullptr;
    }
    if (!sl->occupied) {
        Str copy = StrDup(s);
        if (!copy.s) {
            return nullptr;
        }
        sl->text = copy.s;
        sl->len = copy.len;
        sl->hash = hash;
        sl->fontSize = keyFont;
        sl->maxW = keyMaxW;
        sl->lineH = lineH;
        sl->wrap = wrap ? 1 : 0;
        sl->bold = weight;
        sl->occupied = 1;
        c->used++;
    }
    sl->w = w;
    sl->h = h;
    sl->lastUsed = c->frame;
    if (layout && sl->layout != layout) {
        if (sl->layout) {
            TextLayoutRelease(sl->layout);
        }
        TextLayoutAddRef(layout);
        sl->layout = layout;
    }
    return sl;
}

void TextMeasBeginFrame(PaintCtx* ctx) {
    if (!ctx) {
        return;
    }
    ctx->textCache.frame++;
    if (ctx->textCache.frame == 0) {
        ctx->textCache.frame = 1;
    }
}

void TextMeasEndFrame(PaintCtx* ctx) {
    if (!ctx) {
        return;
    }
    TextMeasCache* c = &ctx->textCache;
    if (!c->slots || c->cap <= 0) {
        return;
    }
    uint32_t frame = c->frame;
    TextMeasSlot* old = (TextMeasSlot*)c->slots;
    int oldCap = c->cap;
    int keep = 0;
    for (int i = 0; i < oldCap; i++) {
        if (old[i].occupied && old[i].lastUsed + 1 >= frame) {
            keep++;
        }
    }
    int newCap = c->cap;
    if (keep * 4 < newCap && newCap > 256) {
        newCap = 256;
        while (newCap < keep * 2) {
            newCap *= 2;
        }
    }
    TextMeasSlot* neu = AllocArray<TextMeasSlot>(newCap);
    if (!neu) {
        return;
    }
    c->slots = neu;
    c->cap = newCap;
    c->used = 0;
    for (int i = 0; i < oldCap; i++) {
        if (!old[i].occupied) {
            continue;
        }
        if (old[i].lastUsed + 1 < frame) {
            TextMeasFreeSlot(&old[i]);
            continue;
        }
        TextMeasInsertMove(c, &old[i]);
    }
    Free(nullptr, old);
}

void TextMeasClear(PaintCtx* ctx) {
    if (!ctx) {
        return;
    }
    TextMeasCache* c = &ctx->textCache;
    TextMeasSlot* slots = (TextMeasSlot*)c->slots;
    if (slots) {
        for (int i = 0; i < c->cap; i++) {
            if (slots[i].occupied) {
                TextMeasFreeSlot(&slots[i]);
            }
        }
        Free(nullptr, slots);
    }
    c->slots = nullptr;
    c->cap = 0;
    c->used = 0;
    c->frame = 0;
}

// Create or reuse a cached shaped run. Caller must TextLayoutRelease.
// `outCached` says whether the cache took a reference of its own, i.e. whether
// the run outlives the caller's; see El::laidLayout.
static TextLayout* TextMeasLayout(PaintCtx* ctx, Str s, float fontSize,
                                  float maxW, bool wrap, uint8_t weight,
                                  float lineH, Size* outSize,
                                  bool* outCached = nullptr) {
    if (outCached) {
        *outCached = false;
    }
    if (outSize) {
        outSize->w = 0;
        outSize->h =
            fontSize > 0 ? fontSize * (lineH > 0 ? lineH : kLineHeight) : 16.f;
    }
    if (!ctx || !ctx->pa || !s.s || s.len <= 0) {
        return nullptr;
    }
    // A run that does not wrap is the same size whatever width it was
    // measured against, which is why the cache key drops maxW for one. The
    // shaped run is not: the platform lays it out inside a box of that width
    // and draws it clipped to the box. Shaping it unconstrained is what makes
    // the key's premise true — a table cell whose min-content width was asked
    // for at one pixel would otherwise keep the one-pixel run it was given,
    // and paint a one-pixel smear where its text belongs. `truncate` does its
    // own cutting at paint time, against the box layout settled on.
    if (!wrap) {
        maxW = 0;
    }
    TextMeasCache* c = &ctx->textCache;
    TextMeasSlot* hit =
        TextMeasFind(c, s, fontSize, maxW, wrap, weight, lineH, nullptr);
    if (hit && hit->layout) {
        hit->lastUsed = c->frame;
        if (outCached) {
            *outCached = true;
        }
        if (outSize) {
            outSize->w = hit->w;
            outSize->h = hit->h;
        }
        TextLayoutAddRef(hit->layout);
        return hit->layout;
    }
    Size size = {};
    TextLayout* layout =
        TextLayoutNew(ctx, s, fontSize, maxW, wrap, weight, lineH, &size);
    if (!layout) {
        return nullptr;
    }
    if (outSize) {
        *outSize = size;
    }
    TextMeasSlot* sl = TextMeasInsert(ctx, s, fontSize, maxW, wrap, weight,
                                      lineH, size.w, size.h, layout);
    if (outCached) {
        *outCached = sl != nullptr;
    }
    return layout;
}

// The size alone, which is all the layout pass ever wants. Going through
// TextMeasLayout for it took a reference on the shaped run and gave it back
// one line later, and an IDWriteTextLayout's AddRef/Release pair is two
// interlocked ops on a shared cache line — 3% of a story frame, because
// taffy asks a text leaf for its size several times per pass and there are
// hundreds of them. On a cache hit the slot already holds the answer.
Size MeasureText(PaintCtx* ctx, Str s, float fontSize, float maxW, bool wrap,
                 int weight, float lineH) {
    Size size = {};
    size.h = fontSize > 0 ? fontSize * (lineH > 0 ? lineH : kLineHeight) : 16.f;
    if (!ctx || !ctx->pa || !s.s || s.len <= 0) {
        return size;
    }
    // Same premise as TextMeasLayout: a run that does not wrap measures the
    // same whatever width it was asked about, so the key drops maxW for one.
    if (!wrap) {
        maxW = 0;
    }
    TextMeasCache* c = &ctx->textCache;
    TextMeasSlot* hit = TextMeasFind(c, s, fontSize, maxW, wrap,
                                     (uint8_t)weight, lineH, nullptr);
    if (hit && hit->layout) {
        hit->lastUsed = c->frame;
        size.w = hit->w;
        size.h = hit->h;
        return size;
    }
    TextLayout* layout = TextMeasLayout(ctx, s, fontSize, maxW, wrap,
                                        (uint8_t)weight, lineH, &size);
    if (layout) {
        TextLayoutRelease(layout);
    }
    return size;
}

bool TextPointAt(PaintCtx* ctx, Str s, float fontSize, float maxW, bool wrap,
                 int off, float* outX, float* outY, float* outH, bool mono,
                 float lineHeight) {
    if (!ctx) {
        return false;
    }
    // An empty line has no layout to measure, and asking for one would fail;
    // the only point in it is its start.
    if (s.len <= 0) {
        *outX = 0;
        *outY = 0;
        *outH = fontSize;
        return true;
    }
    uint8_t weight = mono ? (uint8_t)kFontMono : (uint8_t)0;
    if (off < 0) {
        off = 0;
    }
    if (off > s.len) {
        off = s.len;
    }
    TextLayout* tl = TextMeasLayout(ctx, s, fontSize, maxW, wrap, weight,
                                    lineHeight, nullptr);
    if (!tl) {
        return false;
    }
    Bounds r[32] = {};
    bool ok = false;
    if (s.len == 0) {
        *outX = 0;
        *outY = 0;
        *outH = fontSize;
        ok = true;
    } else if (off > 0) {
        // The trailing edge of everything before it, the way the caret is
        // placed.
        int n = TextLayoutRangeRects(tl, s, 0, off, r, 32);
        if (n > 0) {
            *outX = r[n - 1].x + r[n - 1].w;
            *outY = r[n - 1].y;
            *outH = r[n - 1].h;
            ok = true;
        }
    } else {
        int n = TextLayoutRangeRects(tl, s, 0, s.len, r, 32);
        if (n > 0) {
            *outX = r[0].x;
            *outY = r[0].y;
            *outH = r[0].h;
            ok = true;
        }
    }
    TextLayoutRelease(tl);
    return ok;
}

int TextIndexAt(PaintCtx* ctx, Str s, float fontSize, float maxW, bool wrap,
                float relX, float relY, bool mono, float lineHeight) {
    TextLayout* layout = TextMeasLayout(ctx, s, fontSize, maxW, wrap,
                                        mono ? (uint8_t)kFontMono : (uint8_t)0,
                                        lineHeight, nullptr);
    if (!layout) {
        return 0;
    }
    int off = TextLayoutHitPoint(layout, s, relX, relY);
    TextLayoutRelease(layout);
    return off;
}

// The marked range's underline: the same rects the selection quad is built
// from, one device pixel tall at the bottom of each. Rust hands the run an
// UnderlineStyle instead, which the shaper draws; the rects land in the same
// place and cost no new text machinery.
// The squiggle a wavy underline is: a run of half-period diagonals under the
// glyphs, drawn as one path so the joins are the stroke's own.
static void PaintWavyRun(PaintCtx* ctx, float x, float y, float w, Rgba color) {
    const float kPeriod = 4.f;
    const float kAmp = 1.5f;
    if (w <= 0) {
        return;
    }
    Path* p = PathNew(ctx, false);
    if (!p) {
        return;
    }
    PathMoveTo(p, x, y);
    bool up = true;
    for (float at = kPeriod * 0.5f; at < w; at += kPeriod * 0.5f) {
        PathLineTo(p, x + at, y + (up ? -kAmp : kAmp));
        up = !up;
    }
    PathStroke(ctx, p, 1.f, color);
    PathFree(p);
}

void PaintTextUnderline(PaintCtx* ctx, Str s, float fontSize, float maxW,
                        bool wrap, float x, float y, int u8a, int u8b,
                        Rgba color, bool wavy) {
    if (!ctx || !ctx->rt || color.a == 0 || u8a >= u8b) {
        return;
    }
    TextLayout* layout =
        TextMeasLayout(ctx, s, fontSize, maxW, wrap, 0, 0, nullptr);
    if (!layout) {
        return;
    }
    Bounds rects[32] = {};
    int n = TextLayoutRangeRects(layout, s, u8a, u8b, rects, 32);
    // Just under the glyphs rather than at the foot of the line box, which is
    // where a shaper puts an underline and where the leading would otherwise
    // drop it out of the field altogether.
    float baseline = TextLayoutBaseline(layout);
    for (int i = 0; i < n; i++) {
        float ux = x + rects[i].x;
        float uy = y + rects[i].y + baseline + 1.f;
        if (wavy) {
            PaintWavyRun(ctx, ux, uy + 1.f, rects[i].w, color);
        } else {
            CanvasFillRect(ctx, ux, uy, rects[i].w, 1.f, color);
        }
    }
    TextLayoutRelease(layout);
}

void PaintTextRange(PaintCtx* ctx, Str s, float fontSize, float maxW, bool wrap,
                    uint8_t weight, float lineH, float x, float y, int u8a,
                    int u8b, Rgba color) {
    if (!ctx || !ctx->rt || color.a == 0) {
        return;
    }
    if (u8a > u8b) {
        int t = u8a;
        u8a = u8b;
        u8b = t;
    }
    if (u8a == u8b) {
        return;
    }
    TextLayout* layout =
        TextMeasLayout(ctx, s, fontSize, maxW, wrap, weight, lineH, nullptr);
    if (!layout) {
        return;
    }
    // One rect per line the selection covers; 32 is more lines than any
    // selectable text block here has.
    Bounds rects[32] = {};
    int n = TextLayoutRangeRects(layout, s, u8a, u8b, rects, 32);
    for (int i = 0; i < n; i++) {
        CanvasFillRect(ctx, x + rects[i].x, y + rects[i].y, rects[i].w,
                       rects[i].h, color);
    }
    TextLayoutRelease(layout);
}

// ─── layout ───────────────────────────────────────────────────────────────
//
// The element tree is laid out by src/taffy — the C++ port of the taffy crate
// GPUI itself uses — rather than by an engine of its own. Each frame the El
// tree is translated into a taffy tree, taffy computes it, and the results
// are written back onto the El nodes.
//
// What taffy does not model, and this layer still does:
//
//   - `fixed`: out-of-flow in *window* coordinates. Those elements are
//     re-parented onto the root taffy node as absolutely positioned children,
//     so their insets resolve against the window rather than their El parent.
//   - `anchorBelow` / `anchorAbove` / `anchorCenterX` and the `relative(f)`
//     halves of `left` / `right`: positioning rules gpui-component has and CSS
//     does not. They move an already-laid-out subtree afterwards, which is
//     what the old engine did too.
//   - `scrollX` / `scrollY`: taffy lays a scroll container's content out at
//     the origin and reports how big it is; the offset is applied to the
//     in-flow children as their absolute positions are accumulated.
//   - text, icon, image and progress sizing, which reach the port through a
//     taffy measure function the way Rust's `request_measured_layout` does.
//
// Two deliberate differences from what `Style::to_taffy` does in Rust, both
// noted in port-progress.md:
//
//   - `border` is not given to taffy, so a border still paints over the box
//     rather than reserving space inside it. That is what this tree's widgets
//     were built against; giving taffy the widths would move content by the
//     border width everywhere at the same time as the engine changed.
//   - `maxW` / `maxH` are plain floats whose default of 1e9 means "unset" and
//     maps to auto. `minW` / `minH` default to kAuto, so an element that
//     names no minimum gets CSS's content-based automatic minimum size and an
//     element that says `MinW(0)` gets the zero it asked for.

// The taffy tree is rebuilt every frame but kept between them, so its node
// slots and per-node child arrays are recycled instead of reallocated.
static taffy::TaffyTree gLayoutTree;
static bool gLayoutTreeReady = false;

// The fixed elements found while building this frame's tree, which are
// re-parented onto the root.
static Vec<El*> gLayoutFixed;

static bool RgbaEq(Rgba a, Rgba b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

// Move a laid-out subtree without re-running layout. Positions are absolute,
// so shifting the origin shifts every descendant by the same delta; sizes are
// unaffected.
static void TranslateSubtree(El* e, float dx, float dy) {
    for (El* c = e->first; c; c = c->next) {
        // A fixed element was placed against the window, not against whatever
        // holds it, so sliding an ancestor into place must leave it where it
        // is — and its own subtree with it.
        if (c->style.fixed) {
            continue;
        }
        c->x += dx;
        c->y += dy;
        TranslateSubtree(c, dx, dy);
    }
}

// Move an element that has already been laid out to a new origin.
static void MoveEl(El* c, float cx, float cy) {
    float dx = cx - c->x;
    float dy = cy - c->y;
    if (dx == 0 && dy == 0) {
        return;
    }
    c->x = cx;
    c->y = cy;
    TranslateSubtree(c, dx, dy);
}

// gpui img(..): the box an image takes. Its own pixels are the natural size,
// at one DIP per pixel; a width or a height given by the document wins and the
// other side follows the aspect ratio, which is what html.rs reads out of the
// width / height attributes. Wider than the space it has, it shrinks to fit —
// max_w(relative(1.)) with object_fit(Contain), the pair node.rs gives a
// markdown image.
//
// An image with no size to measure is its alt text instead, measured here so
// the line it sits in is the right height for it.

// The picture's own size, whichever of the two kinds it is: a bitmap's pixels
// or a vector's viewBox. Zero when there is nothing to measure — a fetch
// still running, a missing asset, a format the platform does not read.
static Size ImageNaturalSize(PaintCtx* ctx, El* e) {
    Image* img = ImageForSrc(ctx ? ctx->pa : nullptr, e->imgSrc);
    if (img) {
        return ImageSizePx(img);
    }
    int opsLen = 0;
    const uint8_t* ops = ImageVectorForSrc(e->imgSrc, &opsLen);
    Size vb = {};
    if (ops && DrawOpsViewBox(ops, opsLen, &vb)) {
        return vb;
    }
    return {};
}

static Size LayoutImageSize(PaintCtx* ctx, El* e, float wSpec, float hSpec,
                            float availW, float font) {
    Size px = ImageNaturalSize(ctx, e);
    if (px.w <= 0 || px.h <= 0) {
        Size text =
            MeasureText(ctx, e->text, font, availW > 0 ? availW : 0,
                        e->style.wrap, ElTextWeight(e), e->style.lineHeight);
        return {wSpec > 0 ? wSpec : text.w, hSpec > 0 ? hSpec : text.h};
    }
    float aspect = px.h / px.w;
    float w = wSpec > 0 ? wSpec : (hSpec > 0 ? hSpec / aspect : px.w);
    if (wSpec <= 0 && availW > 0 && w > availW) {
        w = availW;
    }
    float h = hSpec > 0 ? hSpec : w * aspect;
    if (wSpec > 0 && hSpec > 0) {
        h = hSpec;
    }
    return {w, h};
}

// ─── style translation ───────────────────────────────────────────────────

// A gpui length: kAuto means "as big as the content", kFill means "as big as
// the box holding it" (a full-width percentage), anything else is DIPs.
static taffy::Dimension ToDim(float v, float frac) {
    if (frac > 0) {
        return taffy::Dimension::Percent(frac);
    }
    if (v == kFill) {
        return taffy::Dimension::Percent(1.0f);
    }
    if (v == kAuto || v < 0) {
        return taffy::Dimension::Auto();
    }
    return taffy::Dimension::Length(v);
}

// A min-width / min-height: kAuto is the content-based automatic minimum,
// anything else is the length it says, zero included.
static taffy::Dimension ToMinDim(float v) {
    if (v == kAuto || v < 0) {
        return taffy::Dimension::Auto();
    }
    return taffy::Dimension::Length(v);
}

static taffy::LengthPercentageAuto ToInset(float v, float rel) {
    // The pixel and the `relative(f)` halves of one inset cannot both reach
    // taffy without a calc() node, so a mixed pair is finished off by
    // PlaceAnchored below and only the plain cases are handed over here.
    if (rel != 0) {
        return taffy::LengthPercentageAuto::Auto();
    }
    if (v == kAuto) {
        return taffy::LengthPercentageAuto::Auto();
    }
    return taffy::LengthPercentageAuto::Length(v);
}

static taffy::Overflow ToTaffyOverflow(Overflow o) {
    switch (o) {
        case Overflow::Hidden:
            return taffy::Overflow::Hidden;
        case Overflow::Scroll:
            return taffy::Overflow::Scroll;
        default:
            return taffy::Overflow::Visible;
    }
}

static taffy::OptAlignItems ToTaffyAlignItems(Align a) {
    using K = taffy::AlignItemsKeyword;
    switch (a) {
        case Align::Start:
            return taffy::OptAlignItems(taffy::AlignItems{K::Start});
        case Align::Center:
            return taffy::OptAlignItems(taffy::AlignItems{K::Center});
        case Align::End:
            return taffy::OptAlignItems(taffy::AlignItems{K::End});
        default:
            return taffy::OptAlignItems(taffy::AlignItems{K::Stretch});
    }
}

static taffy::FlexDirection ToTaffyFlexDir(FlexDir d) {
    switch (d) {
        case FlexDir::Col:
            return taffy::FlexDirection::Column;
        case FlexDir::RowReverse:
            return taffy::FlexDirection::RowReverse;
        case FlexDir::ColReverse:
            return taffy::FlexDirection::ColumnReverse;
        default:
            return taffy::FlexDirection::Row;
    }
}

static taffy::OptJustifyContent ToTaffyJustify(Justify j) {
    using K = taffy::AlignContentKeyword;
    switch (j) {
        case Justify::Center:
            return taffy::OptJustifyContent(taffy::AlignContent{K::Center});
        case Justify::End:
            return taffy::OptJustifyContent(taffy::AlignContent{K::End});
        case Justify::SpaceBetween:
            return taffy::OptJustifyContent(
                taffy::AlignContent{K::SpaceBetween});
        case Justify::SpaceAround:
            return taffy::OptJustifyContent(
                taffy::AlignContent{K::SpaceAround});
        default:
            return taffy::OptJustifyContent(taffy::AlignContent{K::Start});
    }
}

// Rust's `Style::to_taffy`, for the subset of CSS this tree's Style carries.
static taffy::Style ToTaffyStyle(const El* e) {
    const Style& s = e->style;
    taffy::Style t;
    t.display = s.display == Display::Flex ? taffy::Display::Flex
                                           : taffy::Display::Block;
    t.flexDirection = ToTaffyFlexDir(s.dir);
    t.flexWrap = s.flexWrap ? taffy::FlexWrap::Wrap : taffy::FlexWrap::NoWrap;
    t.alignItems = ToTaffyAlignItems(s.align);
    t.justifyContent = ToTaffyJustify(s.justify);
    t.overflow = {ToTaffyOverflow(s.overflowX), ToTaffyOverflow(s.overflowY)};

    t.size = {ToDim(s.width, s.widthFrac), ToDim(s.height, 0)};
    if (s.aspect > 0) {
        t.aspectRatio = taffy::Some(s.aspect);
    }
    // An unset min is `auto`: a flex item may not shrink below its own
    // content, which is CSS's default and Rust's. An explicit zero is the
    // opposite instruction — `min_w_0()`, "this may shrink past its content"
    // — and it is what a pane holding something wider than the window says so
    // the window's width still wins.
    t.minSize = {ToMinDim(s.minW), ToMinDim(s.minH)};
    // Through ToDim rather than straight to Length: kFill in a max is
    // `max_w(relative(1.))` -- a hundred percent of what holds it, which is
    // how node.rs keeps a picture inside its column -- and a length of -2 is
    // a max of nothing at all, which collapses the box.
    t.maxSize = {s.maxW < 1e9f ? ToDim(s.maxW, 0) : taffy::Dimension::Auto(),
                 s.maxH < 1e9f ? ToDim(s.maxH, 0) : taffy::Dimension::Auto()};

    t.flexGrow = s.flexGrow;
    t.flexShrink = s.flexShrink;
    // An auto basis makes the main size the item's own size style, which is
    // what a plain `W()` means. `flex_1()` names zero instead, and taffy then
    // splits the whole line by the grow factors rather than only the slack.
    t.flexBasis =
        s.flexBasisFrac > 0
            ? taffy::Dimension::Percent(s.flexBasisFrac)
            : (s.flexBasis == kAuto ? taffy::Dimension::Auto()
                                    : taffy::Dimension::Length(s.flexBasis));

    t.padding = {taffy::LengthPercentage::Length(s.pad.left),
                 taffy::LengthPercentage::Length(s.pad.right),
                 taffy::LengthPercentage::Length(s.pad.top),
                 taffy::LengthPercentage::Length(s.pad.bottom)};
    t.gap = {taffy::LengthPercentage::Length(s.gapX),
             taffy::LengthPercentage::Length(s.gapY)};

    if (s.absolute || s.fixed) {
        t.position = taffy::Position::Absolute;
        t.inset = {ToInset(s.absLeft, s.absLeftRel),
                   ToInset(s.absRight, s.absRightRel), ToInset(s.absTop, 0),
                   ToInset(s.absBottom, 0)};
    }
    return t;
}

// ─── measurement ─────────────────────────────────────────────────────────

// What a leaf's measure function is handed, beyond the node itself.
struct LayoutMeasureCtx {
    PaintCtx* ctx;
};

// The width a text run may use: a known width wins, then a definite
// constraint if the run wraps or truncates, else unconstrained.
static float TextMeasureWidth(const El* e, taffy::SizeFOpt known,
                              taffy::SizeAvail avail) {
    if (taffy::IsSome(known.w)) {
        return known.w;
    }
    if (!(e->style.wrap || e->style.truncate)) {
        return 0.0f;
    }
    if (avail.width.IsDefinite()) {
        return avail.width.value > 0 ? avail.width.value : 0.0f;
    }
    // A min-content constraint asks for the narrowest the run can be, which
    // for wrapped text is its longest word — what a one-pixel wrap box gives.
    // A run that only truncates cannot break, so its narrowest is its whole
    // width, which is what an unconstrained measure answers.
    if (avail.width.kind == taffy::AvailableSpace::Kind::MinContent &&
        e->style.wrap) {
        return 1.0f;
    }
    return 0.0f;
}

// Not snapped. gpui ceils what a leaf measures to the device pixel grid
// (`snap_measured_size_to_device_pixels`), and it is tempting to read the raw
// float handed over here as the sub-pixel drift between this tree and that
// one. It is not: gpui runs the *whole* layout in device pixels — available
// space multiplied by the scale factor on the way in, every authored length
// rounded in `to_taffy`, bounds divided back out on the way to paint — and
// ceiling the measure alone imports a quarter of that model. Measured against
// the engine this port replaced, over all 65 story pages, doing so moved
// every one of them further away and tripled the pixels that differ, because
// it quantises text boxes while the padding, gaps and borders around them
// stay where they were. Whole model or none of it; port-progress.md has the
// numbers.
static taffy::SizeF LayoutMeasure(taffy::SizeFOpt known, taffy::SizeAvail avail,
                                  taffy::NodeId node, void* nodeContext,
                                  const taffy::Style* nodeStyle,
                                  void* userData) {
    (void)node;
    (void)nodeStyle;
    El* e = (El*)nodeContext;
    LayoutMeasureCtx* mc = (LayoutMeasureCtx*)userData;
    if (!e) {
        return taffy::SizeF::Zero();
    }
    PaintCtx* ctx = mc ? mc->ctx : nullptr;
    float font = e->laidFont;

    switch (e->kind) {
        case ElKind::Text: {
            float measW = TextMeasureWidth(e, known, avail);
            Size text = {};
            int slot = -1;
            for (int i = 0; i < e->measCount; i++) {
                if (e->measKeyW[i] == measW) {
                    slot = i;
                    break;
                }
            }
            if (slot >= 0) {
                text = e->measSize[slot];
            } else {
                text = MeasureText(ctx, e->text, font, measW, e->style.wrap,
                                   ElTextWeight(e), e->style.lineHeight);
                // Four widths, oldest out. A leaf asked about more than four
                // in one pass simply measures again.
                int at = e->measCount < 4 ? e->measCount++ : e->measNext;
                e->measNext = (uint8_t)((e->measNext + 1) & 3);
                e->measKeyW[at] = measW;
                e->measSize[at] = text;
            }
            return {taffy::UnwrapOr(known.w, text.w),
                    taffy::UnwrapOr(known.h, text.h)};
        }
        case ElKind::Icon:
            return {taffy::UnwrapOr(known.w, 16.0f),
                    taffy::UnwrapOr(known.h, 16.0f)};
        case ElKind::Progress:
            return {taffy::UnwrapOr(known.w, 48.0f),
                    taffy::UnwrapOr(known.h, 8.0f)};
        case ElKind::Image: {
            float availW = avail.width.IsDefinite() ? avail.width.value : 0.0f;
            Size sz =
                LayoutImageSize(ctx, e, taffy::UnwrapOr(known.w, 0.0f),
                                taffy::UnwrapOr(known.h, 0.0f), availW, font);
            return {sz.w, sz.h};
        }
        default:
            return taffy::SizeF::Zero();
    }
}

// A childless Div is still a box with a size; only these kinds have content
// of their own to measure.
static bool ElIsMeasured(const El* e) {
    switch (e->kind) {
        case ElKind::Text:
        case ElKind::Icon:
        case ElKind::Progress:
        case ElKind::Image:
            return true;
        default:
            return false;
    }
}

// ─── building the taffy tree ─────────────────────────────────────────────

// gpui's `img()` does not measure: `Img::request_layout` reads the decoded
// bitmap's size, stamps an aspect ratio on the style, and fills in whichever
// of width and height was auto — from the other one when that one is an
// absolute length, from the bitmap otherwise. Doing the same here is not a
// tidiness: an image left as a measured leaf is asked for its size with the
// cross axis already known, answers with the width that height implies, and
// that width becomes the flex base size the next pass stretches again. A run
// of markdown with a picture in it grew a little on every pass.
//
// An image that cannot be decoded — a remote URL, a format the platform does
// not read — has no size to resolve and stays a measured leaf, so its alt
// text is measured as the text it is. That is our stand-in for gpui's
// `fallback` element.
static void ResolveImageStyle(PaintCtx* ctx, El* e) {
    Size px = ImageNaturalSize(ctx, e);
    if (px.w <= 0 || px.h <= 0) {
        return;
    }
    Style& s = e->style;
    s.aspect = px.w / px.h;
    // An absolute length is the only thing the other axis can be derived
    // from; kFill and the fractional widths are a share of a box that is not
    // settled yet.
    auto absolute = [](float v) { return v != kAuto && v != kFill && v >= 0; };
    if (s.width == kAuto) {
        s.width = absolute(s.height) ? px.w * s.height / px.h : px.w;
    }
    if (s.height == kAuto) {
        s.height = (absolute(s.width) && s.widthFrac == 0)
                       ? px.h * s.width / px.w
                       : px.h;
    }
}

// The style refinement, the inspector's live edit, the inherited font and the
// inherited color, resolved once per element before anything is measured.
// The old engine did this on the way down its own recursion.
static void PrepareEl(PaintCtx* ctx, El* e, float inheritFont, Rgba inheritFg) {
    // The element's own refinement first — a semantic state, which is meant
    // to win over whatever the caller chained on — and then the inspector's
    // live edit, which wins over everything.
    if (e->refineSet) {
        StyleApplyFields(&e->style, e->refine, e->refineSet);
        e->refineSet = 0;
    }
    StyleOverrideApply(e);

    // An explicit size is in DIPs at the default font size and scales with
    // it; an inherited one has been scaled already, by the root or by
    // whichever ancestor set it.
    float font = e->style.fontSize > 0 ? e->style.fontSize * ThemeFontScale()
                                       : inheritFont;
    Rgba fg = e->style.hasColor ? e->style.color : inheritFg;
    // Like HoverBg, this needs a click id of its own: without one the element
    // would match hoverId 0, which means nothing is hovered.
    if (e->style.hasHoverFg && e->clickId && ctx &&
        e->clickId == ctx->hoverId) {
        fg = e->style.hoverFg;
    }
    // `text_color` cascades in GPUI, and this is where. A Text or an Icon
    // resolves its colour when it paints, and what it used to resolve to
    // when it named none was `theme.foreground` — so a container that set a
    // colour coloured its icons and nothing else. An Alert's title is the
    // plainest case: `h_flex().text_color(variant.fg(cx))` around it, and the
    // port drew it black inside a blue box.
    e->style.color = fg;
    e->style.hasColor = true;
    // font_family inherits. Pushing the flag one level down here cascades it
    // through the subtree, since every child is prepared the same way.
    if (e->style.fontMono) {
        for (El* c = e->first; c; c = c->next) {
            c->style.fontMono = true;
        }
    }
    // font_weight inherits the same way — GPUI's `font_medium()` on a row is
    // what makes the string inside it medium, and an accordion's title is one
    // of those. A child that names a weight of its own keeps it; a child that
    // names none takes the one above.
    if (e->style.fontBold || e->style.fontSemibold || e->style.fontMedium) {
        for (El* c = e->first; c; c = c->next) {
            if (c->style.fontBold || c->style.fontSemibold ||
                c->style.fontMedium) {
                continue;
            }
            c->style.fontBold = e->style.fontBold;
            c->style.fontSemibold = e->style.fontSemibold;
            c->style.fontMedium = e->style.fontMedium;
        }
    }
    e->laidFont = font;
    if (e->kind == ElKind::Image) {
        ResolveImageStyle(ctx, e);
    }

    for (El* c = e->first; c; c = c->next) {
        PrepareEl(ctx, c, font, fg);
    }
}

static taffy::NodeId BuildNode(El* e) {
    taffy::Style ts = ToTaffyStyle(e);
    taffy::NodeId id;
    if (ElIsMeasured(e)) {
        id = gLayoutTree.NewLeafWithContext(ts, e);
    } else {
        id = gLayoutTree.NewLeaf(ts);
    }
    e->layoutNode = id.raw;

    for (El* c = e->first; c; c = c->next) {
        if (c->style.fixed) {
            // Placed against the window, so it hangs off the root instead.
            gLayoutFixed.Append(c);
            BuildNode(c);
            continue;
        }
        gLayoutTree.AddChild(id, BuildNode(c));
    }
    return id;
}

// ─── writing the result back ─────────────────────────────────────────────

static void WriteBackEl(PaintCtx* ctx, El* e, float originX, float originY);

static void WriteBackChildren(PaintCtx* ctx, El* e) {
    // A scrolled box slides its in-flow content; an out-of-flow child is
    // pinned to the box and does not move with it, which is what the old
    // engine's PlaceOutOfFlow did.
    float inFlowX = e->x - e->scrollX;
    float inFlowY = e->y - e->scrollY;
    for (El* c = e->first; c; c = c->next) {
        if (c->style.fixed) {
            continue;
        }
        if (c->style.absolute) {
            WriteBackEl(ctx, c, e->x, e->y);
        } else {
            WriteBackEl(ctx, c, inFlowX, inFlowY);
        }
    }
}

static void WriteBackEl(PaintCtx* ctx, El* e, float originX, float originY) {
    const taffy::Layout& l = gLayoutTree
                                 .GetLayout(taffy::NodeId{e->layoutNode});
    e->x = originX + l.location.x;
    e->y = originY + l.location.y;
    e->w = l.size.w;
    e->h = l.size.h;
    e->contentW = l.contentSize.w;
    e->contentH = l.contentSize.h;

    // The shaped run paint wants, taken from the text cache at the size
    // layout settled on. Releasing our reference is safe because a cached run
    // belongs to the cache until TextMeasEndFrame, well after paint.
    if (e->kind == ElKind::Text) {
        bool constrain = e->style.wrap || e->style.truncate;
        float measW = constrain ? e->w : 0.0f;
        e->laidMaxW = measW;
        bool cached = false;
        TextLayout* tl = TextMeasLayout(ctx, e->text, e->laidFont, measW,
                                        e->style.wrap, (uint8_t)ElTextWeight(e),
                                        e->style.lineHeight, nullptr, &cached);
        e->laidLayout = cached ? tl : nullptr;
        if (tl) {
            TextLayoutRelease(tl);
        }
    }

    WriteBackChildren(ctx, e);
}

// The positioning rules gpui-component has and CSS does not: an overlay
// anchored under or over its trigger, one centred on it, and the
// `relative(f)` half of a left/right inset. Each moves a subtree that taffy
// has already sized and placed.
static void PlaceAnchored(El* e, float viewW, float viewH) {
    for (El* c = e->first; c; c = c->next) {
        PlaceAnchored(c, viewW, viewH);
        const Style& s = c->style;
        bool anchored = s.anchorBelow || s.anchorAbove || s.anchorCenterX;
        if (!anchored && (c->style.fixed || !c->style.absolute)) {
            continue;
        }
        if (!anchored && s.absLeftRel == 0 && s.absRightRel == 0) {
            continue;
        }
        float innerW = e->w - e->style.pad.HorizontalAxisSum();
        if (innerW < 0) {
            innerW = 0;
        }
        float ax = c->x;
        float ay = c->y;
        // A `fixed` popup was laid out against the window — which is the
        // point, since that is the width its content had to shape against,
        // the way Rust's Positioner sits in the deferred layer rather than
        // inside the trigger. Where it goes is still the trigger's business,
        // so the inset it named is read off the trigger's box here.
        if (s.fixed && anchored) {
            ax = e->x + (s.absLeft == kAuto ? 0.f : s.absLeft);
            if (s.absRight != kAuto) {
                ax = e->x + e->w - s.absRight - c->w;
            }
            ay = e->y;
        }
        if (s.absLeftRel != 0) {
            float absL = (s.absLeft == kAuto ? 0.f : s.absLeft);
            ax = e->x + e->style.pad.left + absL + innerW * s.absLeftRel;
        }
        if (s.absRightRel != 0) {
            float absR = (s.absRight == kAuto ? 0.f : s.absRight);
            ax = e->x + e->w - e->style.pad.right - absR -
                 innerW * s.absRightRel - c->w;
        }
        if (s.anchorBelow) {
            ay = e->y + e->h + s.anchorGap;
        }
        if (s.anchorAbove) {
            ay = e->y - c->h - s.anchorGap;
        }
        if (s.anchorCenterX) {
            ax = e->x + (e->w - c->w) * 0.5f;
        }
        // positioner.rs `clamp`: whatever the corner worked out, the popup is
        // then pulled back inside the viewport with WINDOW_MARGIN to spare.
        // It never flips — that is the side strategy's job — so a popup with
        // nowhere to go simply sits against the edge.
        if (anchored && viewW > 0 && viewH > 0) {
            float m = kPopupMargin;
            if (ax + c->w > viewW - m) {
                ax = viewW - m - c->w;
            }
            if (ax < m) {
                ax = m;
            }
            if (ay + c->h > viewH - m) {
                ay = viewH - m - c->h;
            }
            if (ay < m) {
                ay = m;
            }
        }
        MoveEl(c, ax, ay);
    }
}

void LayoutEl(PaintCtx* ctx, El* e, float x, float y, float availW,
              float availH, float inheritFont, Rgba inheritFg) {
    if (!e) {
        return;
    }
    if (!gLayoutTreeReady) {
        gLayoutTree.Init(256);
        // GPUI calls `taffy.disable_rounding()`; everything above paint here
        // is DIPs, and the backends snap to the pixel grid themselves.
        gLayoutTree.DisableRounding();
        gLayoutTreeReady = true;
    }
    gLayoutTree.Clear();
    gLayoutFixed.len = 0;

    PrepareEl(ctx, e, inheritFont, inheritFg);

    taffy::NodeId root = BuildNode(e);
    // A `fixed` element resolves its insets against the window, so it hangs
    // off the root rather than off whatever built it.
    for (int i = 0; i < gLayoutFixed.len; i++) {
        gLayoutTree.AddChild(root, taffy::NodeId{gLayoutFixed[i]->layoutNode});
    }

    // Rust's `stretch_auto_size_to_fill`: a root with an auto dimension fills
    // the space it was given, the way the root element of a page fills the
    // viewport.
    taffy::Style rootStyle = gLayoutTree.GetStyle(root);
    bool changed = false;
    if (rootStyle.size.width.IsAuto() && availW > 0) {
        rootStyle.size.width = taffy::Dimension::Length(availW);
        changed = true;
    }
    if (rootStyle.size.height.IsAuto() && availH > 0) {
        rootStyle.size.height = taffy::Dimension::Length(availH);
        changed = true;
    }
    if (changed) {
        gLayoutTree.SetStyle(root, rootStyle);
    }

    taffy::SizeAvail space;
    space.width = availW > 0 ? taffy::AvailableSpace::Definite(availW)
                             : taffy::AvailableSpace::MaxContent();
    space.height = availH > 0 ? taffy::AvailableSpace::Definite(availH)
                              : taffy::AvailableSpace::MaxContent();

    LayoutMeasureCtx mc = {ctx};
    gLayoutTree.ComputeLayoutWithMeasure(root, space, LayoutMeasure, &mc);

    WriteBackEl(ctx, e, x, y);
    // The fixed elements are laid out as children of the root, so their boxes
    // come out in window coordinates already.
    for (int i = 0; i < gLayoutFixed.len; i++) {
        WriteBackEl(ctx, gLayoutFixed[i], 0, 0);
    }
    PlaceAnchored(e, ctx ? ctx->viewW : 0.f, ctx ? ctx->viewH : 0.f);
}

// ─── paint ────────────────────────────────────────────────────────────────

static void FillRound(PaintCtx* ctx, float x, float y, float w, float h,
                      float r, Rgba c) {
    CanvasFillRound(ctx, x, y, w, h, r, c);
}

// The four corners of a box, as one path: a quarter turn at each corner that
// asked for one and a plain corner where it did not. Built here rather than in
// the two backends because the path API is already portable and a rounded box
// is nothing but four arcs — D2D's own rounded rectangle takes one radius, and
// so does cairo's and Core Graphics'.
static void CornersPath(Path* p, float x, float y, float w, float h,
                        const Corners& c) {
    // No corner larger than half the box, the way Rust clamps a radius.
    float lim = (w < h ? w : h) * 0.5f;
    float tl = c.tl < lim ? c.tl : lim;
    float tr = c.tr < lim ? c.tr : lim;
    float br = c.br < lim ? c.br : lim;
    float bl = c.bl < lim ? c.bl : lim;
    float r = x + w;
    float b = y + h;
    PathMoveTo(p, x + tl, y);
    PathLineTo(p, r - tr, y);
    if (tr > 0) {
        PathArcTo(p, r - tr, y + tr, tr, -kPi * 0.5f, 0.f, true);
    }
    PathLineTo(p, r, b - br);
    if (br > 0) {
        PathArcTo(p, r - br, b - br, br, 0.f, kPi * 0.5f, true);
    }
    PathLineTo(p, x + bl, b);
    if (bl > 0) {
        PathArcTo(p, x + bl, b - bl, bl, kPi * 0.5f, kPi, true);
    }
    PathLineTo(p, x, y + tl);
    if (tl > 0) {
        PathArcTo(p, x + tl, y + tl, tl, kPi, kPi * 1.5f, true);
    }
    PathClose(p);
}

// The same two calls as FillRound / DrawRoundStroke, for a box whose corners
// differ. `Style::hasCorners` is what picks between them.
static void FillCorners(PaintCtx* ctx, float x, float y, float w, float h,
                        const Corners& c, Rgba col) {
    if (w <= 0 || h <= 0) {
        return;
    }
    if (c.IsUniform()) {
        CanvasFillRound(ctx, x, y, w, h, c.tl, col);
        return;
    }
    Path* p = PathNew(ctx, true);
    CornersPath(p, x, y, w, h, c);
    PathFill(ctx, p, col);
    PathFree(p);
}

// The same two, for a fill that may be a gradient. A gradient is painted as
// a path rather than a rectangle because the path API already carries one on
// all three backends — D2D's linear-gradient brush, cairo's linear pattern,
// Core Graphics' CGGradient — and a rounded box is nothing but four arcs. A
// solid Background takes the rectangle route it always did.
static void FillBackground(PaintCtx* ctx, float x, float y, float w, float h,
                           float r, const Corners* c, const Background& bg) {
    if (w <= 0 || h <= 0) {
        return;
    }
    if (!bg.gradient) {
        if (c) {
            FillCorners(ctx, x, y, w, h, *c, bg.color);
        } else {
            FillRound(ctx, x, y, w, h, r, bg.color);
        }
        return;
    }
    Point p0 = {}, p1 = {};
    BackgroundLine(bg, Bounds{x, y, w, h}, &p0, &p1);
    // Two stops at the same place have no line to run along; the first stop
    // is what the whole box would be anyway.
    if (fabsf(p1.x - p0.x) < 1e-4f && fabsf(p1.y - p0.y) < 1e-4f) {
        if (c) {
            FillCorners(ctx, x, y, w, h, *c, bg.from.color);
        } else {
            FillRound(ctx, x, y, w, h, r, bg.from.color);
        }
        return;
    }
    Corners uniform = {r, r, r, r};
    Path* p = PathNew(ctx, true);
    CornersPath(p, x, y, w, h, c ? *c : uniform);
    PathFillGradient(ctx, p, p0.x, p0.y, p1.x, p1.y, bg.from.color,
                     bg.to.color);
    PathFree(p);
}

static void StrokeCorners(PaintCtx* ctx, float x, float y, float w, float h,
                          const Corners& c, float stroke, Rgba col) {
    if (w <= 0 || h <= 0) {
        return;
    }
    if (c.IsUniform()) {
        CanvasStrokeRound(ctx, x, y, w, h, c.tl, stroke, col);
        return;
    }
    Path* p = PathNew(ctx, true);
    CornersPath(p, x, y, w, h, c);
    PathStroke(ctx, p, stroke, col);
    PathFree(p);
}

// styled.rs FOCUS_RING_WIDTH and FOCUS_RING_OPACITY.
static const float kFocusRingWidth = 3.f;
static const float kFocusRingOpacity = 0.5f;

static void DrawRoundStroke(PaintCtx* ctx, float x, float y, float w, float h,
                            float r, float stroke, Rgba c) {
    CanvasStrokeRound(ctx, x, y, w, h, r, stroke, c);
}

// Layout lands on fractions of a pixel, which spreads a hairline over two
// rows however it is inset. A border line is snapped to the nearest device
// pixel center so it covers exactly one.
static float EdgeLine(PaintCtx* ctx, float v) {
    float scale = ctx->dpi > 0 ? (float)ctx->dpi / 96.f : 1.f;
    float px = v * scale;
    return (floorf(px) + 0.5f) / scale;
}

// The ends of a border line, snapped to the pixel boundary: a dash pattern
// starts at the path's start, so a fractional one smears every dash.
static float EdgeEnd(PaintCtx* ctx, float v) {
    float scale = ctx->dpi > 0 ? (float)ctx->dpi / 96.f : 1.f;
    return floorf(v * scale + 0.5f) / scale;
}

static void DrawLine(PaintCtx* ctx, float x1, float y1, float x2, float y2,
                     float stroke, Rgba c) {
    CanvasLine(ctx, x1, y1, x2, y2, stroke, c);
}

static void DrawTextAt(PaintCtx* ctx, Str s, float x, float y, float w, float h,
                       float fontSize, Rgba c, bool truncate, bool wrap = false,
                       float measMaxW = -1.f, int weight = 0, float lineH = 0) {
    if (!s.s || s.len <= 0 || !ctx->pa) {
        return;
    }
    (void)w;
    (void)h;
    // A wrapping run is shaped to the width it wraps at; a truncating one is
    // shaped to the width it is cut at, which is what gives the backend
    // something to put the ellipsis against. Everything else is unconstrained.
    float keyW = wrap ? (measMaxW >= 0 ? measMaxW : (w > 0 ? w : 0))
                      : (truncate && w > 0 ? w : 0);
    TextLayout* layout = TextMeasLayout(ctx, s, fontSize, keyW, wrap,
                                        (uint8_t)weight, lineH, nullptr);
    if (!layout) {
        return;
    }
    TextLayoutDraw(ctx, layout, x, y, c, truncate, truncate ? keyW : 0.f);
    TextLayoutRelease(layout);
}

void DrawTextBaseline(PaintCtx* ctx, Str s, float x, float baselineY,
                      float fontSize, Rgba color, int weight) {
    if (!s.s || s.len <= 0 || !ctx || !ctx->pa) {
        return;
    }
    TextLayout* layout =
        TextMeasLayout(ctx, s, fontSize, 0, false, (uint8_t)weight, 0, nullptr);
    if (!layout) {
        return;
    }
    TextLayoutDraw(ctx, layout, x, baselineY - TextLayoutBaseline(layout),
                   color, false);
    TextLayoutRelease(layout);
}

// The value domain a chart's y axis is scaled to: what the caller named, or
// the extent of the data — which is what a ScaleLinear over it comes to.
static void ChartDomain(const ChartSeries& c, float* outMin, float* outMax) {
    if (c.domainMin != 0 || c.domainMax != 0) {
        *outMin = c.domainMin;
        *outMax = c.domainMax;
        return;
    }
    float lo = 0;
    float hi = 0;
    bool seen = false;
    for (int i = 0; i < c.n; i++) {
        const float* series[4] = {c.ys, c.opens, c.highs, c.lows};
        for (int k = 0; k < 4 + c.nMore; k++) {
            const float* ys = k < 4 ? series[k] : c.more[k - 4].ys;
            if (!ys) {
                continue;
            }
            float v = ys[i];
            if (!seen || v < lo) {
                lo = v;
            }
            if (!seen || v > hi) {
                hi = v;
            }
            seen = true;
        }
    }
    if (!seen || hi <= lo) {
        *outMin = 0;
        *outMax = hi > 0 ? hi : 1;
        return;
    }
    // A bar, an area and a radar are read against a baseline, so their domain
    // starts at zero unless the data goes below it. A line or a candle is
    // read against itself, so it keeps the extent of its own values with a
    // little air either side.
    if (c.kind == ChartKind::Line || c.kind == ChartKind::Candlestick) {
        float pad = (hi - lo) * 0.1f;
        *outMin = lo - pad;
        *outMax = hi + pad;
        return;
    }
    *outMin = lo > 0 ? 0 : lo;
    *outMax = hi;
}

// StrokeStyle, as the run of segments after the opening move_to. Natural is
// the Catmull-Rom the plot draws by default, turned into the cubic Beziers a
// path can carry; StepAfter holds each value until the next point's x, and
// leaves off the last riser the way Rust's windows(2) loop does.
template <typename FX, typename FY>
static void ChartRun(Path* p, const ChartSeries& c, FX Xat, FY Yat,
                     const float* ys, int n) {
    if (n < 2) {
        return;
    }
    if (c.strokeStyle == ChartStroke::Linear) {
        for (int i = 1; i < n; i++) {
            PathLineTo(p, Xat(i), Yat(ys[i]));
        }
        return;
    }
    if (c.strokeStyle == ChartStroke::StepAfter) {
        for (int i = 0; i + 1 < n; i++) {
            PathLineTo(p, Xat(i + 1), Yat(ys[i]));
            if (i < n - 2) {
                PathLineTo(p, Xat(i + 1), Yat(ys[i + 1]));
            }
        }
        return;
    }
    for (int i = 0; i + 1 < n; i++) {
        int i0 = i == 0 ? 0 : i - 1;
        int i3 = i + 2 < n ? i + 2 : n - 1;
        float x0 = Xat(i0), y0 = Yat(ys[i0]);
        float x1 = Xat(i), y1 = Yat(ys[i]);
        float x2 = Xat(i + 1), y2 = Yat(ys[i + 1]);
        float x3 = Xat(i3), y3 = Yat(ys[i3]);
        PathCubicTo(p, x1 + (x2 - x0) / 6.f, y1 + (y2 - y0) / 6.f,
                    x2 - (x3 - x1) / 6.f, y2 - (y3 - y1) / 6.f, x2, y2);
    }
}

// A row chart writes its band names down one side and its values at the far
// end of each bar, so the value axis gives up a gutter at each end. Rust
// measures the widest of each; a fixed pair is close enough at these sizes.
const float kBarRowBandGap = 52.f;
const float kBarRowValueGap = 36.f;

// One bar: which edge it grows from, what it is filled with, and the value
// written at its growing end.
static void DrawBar(PaintCtx* ctx, const ChartSeries& c, int i, float bx,
                    float bw, float x, float y, float w, float plotH, float lo,
                    float hi, const Theme& th) {
    float t = hi > lo ? (c.ys[i] - lo) / (hi - lo) : 0.f;
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    // Where the bar starts, which is the bottom unless a stack put it on top
    // of the series below.
    float t0 = 0;
    if (c.bases) {
        t0 = hi > lo ? (c.bases[i] - lo) / (hi - lo) : 0.f;
        t0 = t0 < 0 ? 0 : (t0 > 1 ? 1 : t0);
    }
    bool horizontal =
        c.barAlign == BarAlign::Left || c.barAlign == BarAlign::Right;
    // The band runs across the plot for a column chart and down it for a row
    // one, so the two sides of the box swap with the alignment.
    float rx = 0, ry = 0, rw = 0, rh = 0;
    if (horizontal) {
        float bandY = y + (bx - x) * (plotH / (w > 0 ? w : 1));
        float bandH = bw * (plotH / (w > 0 ? w : 1));
        // The value axis runs between the two gutters, from the side the
        // bars are anchored to.
        float ax = x + (c.barAlign == BarAlign::Left ? kBarRowBandGap
                                                     : kBarRowValueGap);
        float aw = w - kBarRowBandGap - kBarRowValueGap;
        if (aw < 1) {
            aw = 1;
        }
        float len = aw * (t - t0);
        if (len < 1) {
            len = 1;
        }
        rx = c.barAlign == BarAlign::Left ? ax + aw * t0
                                          : ax + aw - aw * t0 - len;
        ry = bandY;
        rw = len;
        rh = bandH < 1 ? 1 : bandH;
    } else {
        float span = plotH - 10.f;
        float len = span * (t - t0);
        if (len < 1) {
            len = 1;
        }
        rx = bx;
        rw = bw;
        rh = len;
        ry = c.barAlign == BarAlign::Top ? y + 10.f + span * t0
                                         : y + plotH - span * t0 - len;
    }
    Rgba fill = c.barFills ? c.barFills[i] : c.stroke;
    if (c.barGradient) {
        // fill_gradient: across the chart's own range by default, so a tall
        // bar reaches further up the ramp than a short one; per-bar runs the
        // whole ramp inside every bar.
        Rgba from = c.barFillFrom;
        Rgba to = c.barFillTo;
        if (!c.barGradientPerBar && !c.barGradientDiagonal) {
            // The stop the bar actually reaches, as a mix of the two ends.
            Rgba hit = RgbaMix(from, to, t);
            to = hit;
        }
        // The ramp runs along the bar, which is top-to-bottom for a column
        // and left-to-right for a row.
        Path* box = PathNew(ctx, true);
        if (box) {
            PathMoveTo(box, rx, ry);
            PathLineTo(box, rx + rw, ry);
            PathLineTo(box, rx + rw, ry + rh);
            PathLineTo(box, rx, ry + rh);
            PathClose(box);
            if (c.barGradientDiagonal) {
                // Where the bar's two corners fall along the plot's
                // bottom-left to top-right diagonal, as a fraction of it.
                float pw = w > 1e-6f ? w : 1e-6f;
                float ph = plotH > 1e-6f ? plotH : 1e-6f;
                float denom = pw * pw + ph * ph;
                auto project = [&](float px, float py) {
                    return ((px - x) * pw + (ph - (py - y)) * ph) / denom;
                };
                // RgbaMix(a, b, t) is a*t + b*(1-t), so sampling a ramp that
                // runs `from` to `to` at p means mixing the far stop in at p.
                auto sample = [&](float pos) { return RgbaMix(to, from, pos); };
                PathFillGradient(ctx, box, rx, ry + rh, rx + rw, ry,
                                 sample(project(rx, ry + rh)),
                                 sample(project(rx + rw, ry)));
            } else if (horizontal) {
                PathFillGradient(ctx, box, rx, ry, rx + rw, ry,
                                 c.barAlign == BarAlign::Left ? from : to,
                                 c.barAlign == BarAlign::Left ? to : from);
            } else {
                PathFillGradientV(ctx, box, ry, ry + rh,
                                  c.barAlign == BarAlign::Top ? to : from,
                                  c.barAlign == BarAlign::Top ? from : to);
            }
            PathFree(box);
        }
    } else {
        FillRound(ctx, rx, ry, rw, rh, c.barRadius, fill);
    }
    if (!c.barLabels) {
        return;
    }
    // label(..): the value at the end the bar grew to, just inside it.
    Str text = fmt("%.0f", (double)c.ys[i]);
    if (horizontal) {
        float tx = c.barAlign == BarAlign::Left ? rx + rw + 4 : rx - 34;
        DrawTextAt(ctx, text, tx, ry + rh * 0.5f - 7.f, 30, 14, 10, th.mutedFg,
                   c.barAlign != BarAlign::Left);
    } else {
        float ty = c.barAlign == BarAlign::Top ? ry + rh + 2 : ry - 14.f;
        DrawTextAt(ctx, text, rx + rw * 0.5f - 20.f, ty, 40, 14, 10, th.mutedFg,
                   true);
    }
}

// One shaped layout, painted a run at a time: every span's range is clipped
// to the rects it covers and the whole run is drawn inside that clip in the
// span's colour, so each glyph is drawn exactly once and no two colours
// overlap on the same pixels. What the spans leave over is drawn the same way
// in the element's own colour.
static void PaintTextSpans(PaintCtx* ctx, El* e, float font, Rgba base) {
    float maxW = e->laidMaxW > 0 ? e->laidMaxW : e->w;
    TextLayout* layout =
        TextMeasLayout(ctx, e->text, font, maxW, e->style.wrap, ElTextWeight(e),
                       e->style.lineHeight, nullptr, nullptr);
    if (!layout) {
        DrawTextAt(ctx, e->text, e->x, e->y, e->w, e->h, font, base,
                   e->style.truncate, e->style.wrap, e->laidMaxW,
                   ElTextWeight(e), e->style.lineHeight);
        return;
    }
    Bounds rects[32] = {};
    // The washes go under every glyph, so they all go down first.
    for (int i = 0; i < e->nSpans; i++) {
        const TextSpan& sp = e->spans[i];
        if (sp.bg.a == 0 || sp.hi <= sp.lo) {
            continue;
        }
        int n = TextLayoutRangeRects(layout, e->text, sp.lo, sp.hi, rects, 32);
        for (int r = 0; r < n; r++) {
            CanvasFillRect(ctx, e->x + rects[r].x, e->y + rects[r].y,
                           rects[r].w, rects[r].h, sp.bg);
        }
    }
    // The glyphs, one partition at a time.
    int at = 0;
    for (int i = 0; i <= e->nSpans; i++) {
        int lo = i < e->nSpans ? e->spans[i].lo : e->text.len;
        int hi = i < e->nSpans ? e->spans[i].hi : e->text.len;
        if (lo > at) {
            // What the spans left over, in the element's own colour.
            int n = TextLayoutRangeRects(layout, e->text, at, lo, rects, 32);
            for (int r = 0; r < n; r++) {
                CanvasPushClip(ctx, e->x + rects[r].x, e->y + rects[r].y,
                               rects[r].w, rects[r].h);
                TextLayoutDraw(ctx, layout, e->x, e->y, base, false);
                CanvasPopClip(ctx);
            }
        }
        if (i >= e->nSpans || hi <= lo) {
            at = lo > at ? lo : at;
            continue;
        }
        int n = TextLayoutRangeRects(layout, e->text, lo, hi, rects, 32);
        for (int r = 0; r < n; r++) {
            CanvasPushClip(ctx, e->x + rects[r].x, e->y + rects[r].y,
                           rects[r].w, rects[r].h);
            TextLayoutDraw(ctx, layout, e->x, e->y, e->spans[i].color, false);
            CanvasPopClip(ctx);
        }
        at = hi;
    }
    // The rules last, so nothing paints over them.
    for (int i = 0; i < e->nSpans; i++) {
        const TextSpan& sp = e->spans[i];
        if (!sp.underline || sp.hi <= sp.lo) {
            continue;
        }
        PaintTextUnderline(ctx, e->text, font, maxW, e->style.wrap, e->x, e->y,
                           sp.lo, sp.hi, sp.color, sp.wavy);
    }
    TextLayoutRelease(layout);
}

static void DrawChart(PaintCtx* ctx, El* e) {
    const Theme& th = ThemeNow();
    float x = e->x;
    float y = e->y;
    float w = e->w;
    float h = e->h;
    const float axisGap = 18.f;
    float plotH = h - axisGap;
    if (plotH < 8 || w < 8) {
        return;
    }
    const ChartSeries& c = e->chart;
    int n = c.n;
    const float* ys = c.ys;

    // A radar has no axis along the bottom: its grid is the rings the values
    // are plotted on.
    if (c.kind == ChartKind::Radar) {
        if (!ys || n < 3) {
            return;
        }
        float lo = 0;
        float hi = 0;
        ChartDomain(c, &lo, &hi);
        float cx = x + w * 0.5f;
        float cy = y + h * 0.5f;
        // resolve_outer_radius: two fifths of the box's height, and the
        // caller's own radius where it gave one.
        float radius = h * 0.4f;
        if (c.radarRadius > 0) {
            radius = c.radarRadius;
        }
        // "The domain includes zero so non-negative data starts at the
        // center" — radar_chart.rs chains a zero into the scale's domain, so
        // the smallest value is a short spoke rather than a point on the hub.
        if (lo > 0) {
            lo = 0;
        }
        if (hi < 0) {
            hi = 0;
        }
        if (radius < 8) {
            return;
        }
        int levels = c.gridLevels > 0 ? c.gridLevels : 4;
        // The rings, and a spoke out to every axis. An overlaid series draws
        // on the rings the first one put down.
        for (int ring = 1; ring <= (c.overlay ? 0 : levels); ring++) {
            float rr = radius * (float)ring / (float)levels;
            Path* p = PathNew(ctx, false);
            if (!p) {
                break;
            }
            for (int i = 0; i <= n; i++) {
                float a = -1.5707963f + 6.2831853f * (float)(i % n) / (float)n;
                float px = cx + rr * cosf(a);
                float py = cy + rr * sinf(a);
                if (i == 0) {
                    PathMoveTo(p, px, py);
                } else {
                    PathLineTo(p, px, py);
                }
            }
            PathStroke(ctx, p, 1.f, th.border);
            PathFree(p);
        }
        for (int i = 0; i < (c.overlay ? 0 : n); i++) {
            float a = -1.5707963f + 6.2831853f * (float)i / (float)n;
            DrawLine(ctx, cx, cy, cx + radius * cosf(a), cy + radius * sinf(a),
                     1.f, th.border);
        }
        // The values themselves, as one closed shape.
        Path* shape = PathNew(ctx, true);
        if (shape) {
            for (int i = 0; i < n; i++) {
                float t = hi > lo ? (ys[i] - lo) / (hi - lo) : 0.f;
                if (t < 0) {
                    t = 0;
                }
                if (t > 1) {
                    t = 1;
                }
                float a = -1.5707963f + 6.2831853f * (float)i / (float)n;
                float px = cx + radius * t * cosf(a);
                float py = cy + radius * t * sinf(a);
                if (i == 0) {
                    PathMoveTo(shape, px, py);
                } else {
                    PathLineTo(shape, px, py);
                }
            }
            PathClose(shape);
            PathFill(ctx, shape, c.fillTop);
            PathStroke(ctx, shape, 2.f, c.stroke);
            PathFree(shape);
        }
        // dot(): a mark on every vertex of the ring.
        if (c.dot) {
            for (int i = 0; i < n; i++) {
                float t = hi > lo ? (ys[i] - lo) / (hi - lo) : 0.f;
                t = t < 0 ? 0 : (t > 1 ? 1 : t);
                float a = -1.5707963f + 6.2831853f * (float)i / (float)n;
                float px = cx + radius * t * cosf(a);
                float py = cy + radius * t * sinf(a);
                FillRound(ctx, px - 3.f, py - 3.f, 6.f, 6.f, 3.f, c.stroke);
            }
        }
        if (c.labels && !c.overlay) {
            // label_anchor: the label ring is DEFAULT_LABEL_GAP past the
            // outer one, and a label takes its alignment from the side it is
            // on — left of the anchor going left, right of it going right,
            // and centred at twelve and six o'clock.
            const float kLabelGap = 10.f;
            for (int i = 0; i < n; i++) {
                float a = -1.5707963f + 6.2831853f * (float)i / (float)n;
                float dx = cosf(a);
                float px = cx + (radius + kLabelGap) * dx;
                float py = cy + (radius + kLabelGap) * sinf(a);
                Str label = Str(c.labels[i]);
                float tw = MeasureText(ctx, label, 10, 0).w;
                float tx = px;
                if (dx < -1e-3f) {
                    tx = px - tw;
                } else if (dx <= 1e-3f) {
                    tx = px - tw * 0.5f;
                }
                DrawTextAt(ctx, label, tx, py - 5.f, tw, 14, 10, th.mutedFg,
                           false);
            }
        }
        return;
    }

    // A row chart's value axis runs across rather than up, so its grid and
    // its band names turn with it.
    bool barRow = c.kind == ChartKind::Bar && (c.barAlign == BarAlign::Left ||
                                               c.barAlign == BarAlign::Right);

    // An overlay series draws over the grid and axis the first one drew.
    if (!c.overlay) {
        const float kGridDash[2] = {4.f, 2.f};
        if (barRow) {
            for (int i = 1; i <= 4; i++) {
                float gx = x + w * (i / 4.f);
                CanvasLine(ctx, gx, y, gx, y + plotH, 1.f, th.border,
                           kGridDash);
            }
        } else {
            for (int i = 0; i <= 3; i++) {
                float gy = y + plotH * (i / 4.f);
                CanvasLine(ctx, x, gy, x + w, gy, 1.f, th.border, kGridDash);
            }
            DrawLine(ctx, x, y + plotH, x + w, y + plotH, 1.f, th.border);
        }
    }

    if (!ys || n <= 0) {
        return;
    }
    float lo = 0;
    float hi = 0;
    ChartDomain(c, &lo, &hi);

    auto Xat = [&](int i) -> float {
        if (n <= 1) {
            return x + w * 0.5f;
        }
        return x + (w * (float)i / (float)(n - 1));
    };
    auto Yat = [&](float v) -> float {
        float t = hi > lo ? (v - lo) / (hi - lo) : 0.f;
        if (t < 0) {
            t = 0;
        }
        if (t > 1) {
            t = 1;
        }
        return y + 10.f + (1.f - t) * (plotH - 10.f);
    };

    if (c.kind == ChartKind::Bar || c.kind == ChartKind::Candlestick) {
        // ScaleBand: every point takes a band of the width, with the padding
        // between them coming off each one.
        const float range[2] = {0.f, w};
        component::ScaleBand band = component::ScaleBand::New(n, range, 2);
        band.paddingInner = c.bandPadding;
        band.paddingOuter = c.bandPadding * 0.5f;
        float bw = band.BandWidth();
        if (bw < 1) {
            bw = 1;
        }
        for (int i = 0; i < n; i++) {
            float bx = 0;
            if (!band.Tick(i, &bx)) {
                continue;
            }
            bx += x;
            if (c.kind == ChartKind::Bar) {
                DrawBar(ctx, c, i, bx, bw, x, y, w, plotH, lo, hi, th);
                continue;
            }
            // A candle: the wick from low to high, and the body between open
            // and close, colored by which way it closed.
            float open = c.opens ? c.opens[i] : ys[i];
            float close = ys[i];
            float high = c.highs ? c.highs[i] : (open > close ? open : close);
            float low = c.lows ? c.lows[i] : (open < close ? open : close);
            Rgba color = close >= open ? c.up : c.down;
            float mid = bx + bw * 0.5f;
            DrawLine(ctx, mid, Yat(high), mid, Yat(low), 1.f, color);
            float top = Yat(open > close ? open : close);
            float bot = Yat(open > close ? close : open);
            float bh = bot - top;
            if (bh < 1) {
                bh = 1;
            }
            // body_width_ratio: the body is that much of the band, centred
            // on the wick.
            float ratio = c.bodyWidthRatio > 0 ? c.bodyWidthRatio : 0.8f;
            float bodyW = bw * ratio;
            if (bodyW < 1) {
                bodyW = 1;
            }
            FillRound(ctx, mid - bodyW * 0.5f, top, bodyW, bh, 1.f, color);
        }
    } else {
        // Area and line are the same run of points; only the area fills what
        // is under it. Every series is that run again over the same axes, in
        // the order the caller named them, so a later one draws over an
        // earlier one the way Rust's do.
        auto Band = [&](const float* vs, Rgba stroke, Rgba fillTop,
                        Rgba fillBot) {
            if (!vs) {
                return;
            }
            if (c.kind == ChartKind::Area) {
                Path* area = PathNew(ctx, true);
                if (area) {
                    PathMoveTo(area, Xat(0), y + plotH);
                    PathLineTo(area, Xat(0), Yat(vs[0]));
                    ChartRun(area, c, Xat, Yat, vs, n);
                    PathLineTo(area, Xat(n - 1), y + plotH);
                    PathClose(area);
                    PathFillGradientV(ctx, area, y, y + plotH, fillTop,
                                      fillBot);
                    PathFree(area);
                }
            }
            if (n == 1) {
                DrawLine(ctx, x, Yat(vs[0]), x + w, Yat(vs[0]), 2.f, stroke);
            } else {
                Path* line = PathNew(ctx, false);
                if (line) {
                    PathMoveTo(line, Xat(0), Yat(vs[0]));
                    ChartRun(line, c, Xat, Yat, vs, n);
                    PathStroke(ctx, line, 2.f, stroke);
                    PathFree(line);
                }
            }
            // dot(): a filled mark on every point.
            if (c.dot) {
                for (int i = 0; i < n; i++) {
                    FillRound(ctx, Xat(i) - 3.f, Yat(vs[i]) - 3.f, 6.f, 6.f,
                              3.f, stroke);
                }
            }
        };
        Band(ys, c.stroke, c.fillTop, c.fillBot);
        for (int k = 0; k < c.nMore; k++) {
            const ChartSeriesExtra& more = c.more[k];
            Band(more.ys, more.stroke, more.fillTop, more.fillBot);
        }
    }

    // The crosshair and the tooltip: a chart that asked for them shows what
    // the pointer is over. Rust hangs this off a hover state; the pointer's
    // position is already in the paint context here, so the chart reads it.
    if (c.tooltip && ctx->mouseX >= x && ctx->mouseX <= x + w &&
        ctx->mouseY >= y && ctx->mouseY <= y + plotH) {
        int index = 0;
        float lineX = ctx->mouseX;
        if (c.kind == ChartKind::Bar || c.kind == ChartKind::Candlestick) {
            const float range[2] = {0.f, w};
            component::ScaleBand band = component::ScaleBand::New(n, range, 2);
            band.paddingInner = c.bandPadding;
            band.paddingOuter = c.bandPadding * 0.5f;
            index = band.LeastIndex(ctx->mouseX - x);
            float bx = 0;
            if (band.Tick(index, &bx)) {
                lineX = x + bx + band.BandWidth() * 0.5f;
            }
        } else {
            float t = n > 1 ? (ctx->mouseX - x) / (w / (float)(n - 1)) : 0.f;
            index = (int)(t + 0.5f);
            if (index < 0) {
                index = 0;
            }
            if (index > n - 1) {
                index = n - 1;
            }
            lineX = Xat(index);
        }
        // CrossLine: a dashed hairline down the plot, and a dot on the value.
        const float kCrossDash[2] = {4.f, 3.f};
        CanvasLine(ctx, lineX, y, lineX, y + plotH, 1.f, th.border, kCrossDash);
        float dotY = Yat(ys[index]);
        FillRound(ctx, lineX - 3.f, dotY - 3.f, 6.f, 6.f, 3.f, c.stroke);
        for (int k = 0; k < c.nMore; k++) {
            const ChartSeriesExtra& more = c.more[k];
            if (more.ys) {
                FillRound(ctx, lineX - 3.f, Yat(more.ys[index]) - 3.f, 6.f, 6.f,
                          3.f, more.stroke);
            }
        }

        // The box hugs the cursor and flips toward the middle past halfway,
        // which is what keeps it inside the plot. Every series names its own
        // line, the way Rust's tooltip lists them.
        Str title = c.labels ? Str(c.labels[index]) : fmt("%d", index);
        Str value = c.name.s ? fmt("%s  %.1f", c.name, (double)ys[index])
                             : fmt("%.1f", (double)ys[index]);
        Size titleSz = MeasureText(ctx, title, 11, 200);
        Size valueSz = MeasureText(ctx, value, 11, 200);
        float boxW = (titleSz.w > valueSz.w ? titleSz.w : valueSz.w) + 16.f;
        float boxH = titleSz.h + valueSz.h + 12.f;
        // The extra lines, measured before the box is drawn so it holds them.
        Str extra[4] = {};
        int nExtra = c.nMore < 4 ? c.nMore : 4;
        for (int k = 0; k < nExtra; k++) {
            const ChartSeriesExtra& more = c.more[k];
            extra[k] = more.name.s
                           ? fmt("%s  %.1f", more.name, (double)more.ys[index])
                           : fmt("%.1f", (double)more.ys[index]);
            Size sz = MeasureText(ctx, extra[k], 11, 200);
            if (sz.w + 16.f > boxW) {
                boxW = sz.w + 16.f;
            }
            boxH += sz.h;
        }
        Point at = component::PlotTooltipPlace(
            {ctx->mouseX - x, ctx->mouseY - y}, {w, plotH}, {boxW, boxH}, 8.f);
        FillRound(ctx, x + at.x, y + at.y, boxW, boxH, 6.f, th.background);
        DrawRoundStroke(ctx, x + at.x, y + at.y, boxW, boxH, 6.f, 1.f,
                        th.border);
        DrawTextAt(ctx, title, x + at.x + 8, y + at.y + 4, boxW, titleSz.h, 11,
                   th.foreground, false);
        DrawTextAt(ctx, value, x + at.x + 8, y + at.y + 6 + titleSz.h, boxW,
                   valueSz.h, 11, th.mutedFg, false);
        float lineY = y + at.y + 6 + titleSz.h + valueSz.h;
        for (int k = 0; k < nExtra; k++) {
            DrawTextAt(ctx, extra[k], x + at.x + 8, lineY, boxW, valueSz.h, 11,
                       th.mutedFg, false);
            lineY += valueSz.h;
        }
    }

    // x labels every tickMargin
    int step = c.tickMargin;
    if (step < 1) {
        step = 15;
    }
    if (c.overlay) {
        return;
    }
    // build_point_x_labels keeps the point whose one-based index divides by
    // the margin, so a margin of eight names the eighth point and not the
    // first. The name is centred on its tick, except at the two ends, where
    // it is pulled inside the plot rather than hung over the edge.
    for (int i = 0; i < n; i++) {
        if (step > 1 && ((i + 1) % step) != 0) {
            continue;
        }
        float lx = Xat(i) - 16;
        float ly = y + plotH + 2;
        float lw = 60;
        bool centered = false;
        if (c.kind == ChartKind::Bar || c.kind == ChartKind::Candlestick) {
            // A band's name sits under the band, not under a point — or in
            // the gutter beside it, when the bands run down the side.
            const float range[2] = {0.f, w};
            component::ScaleBand band = component::ScaleBand::New(n, range, 2);
            band.paddingInner = c.bandPadding;
            band.paddingOuter = c.bandPadding * 0.5f;
            float bx = 0;
            if (band.Tick(i, &bx)) {
                if (barRow) {
                    lw = kBarRowBandGap - 6.f;
                    lx = c.barAlign == BarAlign::Left
                             ? x
                             : x + w - kBarRowBandGap + 6.f;
                    ly = y +
                         (bx + band.BandWidth() * 0.5f) *
                             (plotH / (w > 0 ? w : 1)) -
                         7.f;
                    // A left-anchored row's names end right up against the
                    // bars, so they are centred in the gutter rather than
                    // starting at its left edge.
                    centered = c.barAlign == BarAlign::Left;
                } else {
                    lx = x + bx + band.BandWidth() * 0.5f - 16.f;
                }
            }
        }
        Str label = c.labels ? Str(c.labels[i]) : Str(fmt("%ds", i));
        if (!centered && c.kind != ChartKind::Bar &&
            c.kind != ChartKind::Candlestick) {
            // TextAlign::Left at the first point, Right at the last, Center
            // in between — measured, so the box the name is put in is the
            // width the name actually takes.
            Size ls = MeasureText(ctx, label, 10, 0, false, 0, 0);
            float tick = Xat(i);
            lx = i == 0         ? tick
                 : (i == n - 1) ? tick - ls.w
                                : tick - ls.w * 0.5f;
            lw = ls.w;
        }
        DrawTextAt(ctx, label, lx, ly, lw, 16, 10, th.mutedFg, centered);
    }
}

static void PaintElNode(PaintCtx* ctx, El* e, bool skipOverlay);

static bool IsOverlay(El* e) {
    return e->style.fixed || e->style.deferred;
}

// GPUI paints deferred elements after the tree they came from, so a dialog or
// an open dropdown covers the page instead of being covered by the siblings
// that follow it. Painting last also hit-tests first: HitTestRect walks the
// rects backwards.
static void PaintOverlays(PaintCtx* ctx, El* e) {
    if (!e) {
        return;
    }
    if (IsOverlay(e)) {
        PaintElNode(ctx, e, false);
        return;
    }
    for (El* c = e->first; c; c = c->next) {
        PaintOverlays(ctx, c);
    }
}

// InputElement's cursor_bounds: where the caret sits inside the run this
// element painted. Rust measures it in prepaint from the shaped line and
// paints a quad there; the shaped line is already in hand here, so the two
// steps fold together. A run with no text puts the caret at its left edge,
// which is where an empty field with a placeholder shows it.
static void PaintCaret(PaintCtx* ctx, El* e, float font) {
    if (e->caretOff < 0 || e->caretColor.a == 0) {
        return;
    }
    float x = e->x;
    float y = e->y;
    float h = e->h;
    if (e->text.s && e->text.len > 0) {
        float maxW = e->laidMaxW > 0 ? e->laidMaxW : e->w;
        TextLayout* tl = TextMeasLayout(ctx, e->text, font, maxW, e->style.wrap,
                                        0, 0, nullptr);
        if (tl) {
            Bounds r[32] = {};
            int off = e->caretOff;
            if (off > e->text.len) {
                off = e->text.len;
            }
            int n = 0;
            if (off > 0) {
                // The trailing edge of everything before it.
                n = TextLayoutRangeRects(tl, e->text, 0, off, r, 32);
                if (n > 0) {
                    x = e->x + r[n - 1].x + r[n - 1].w;
                    y = e->y + r[n - 1].y;
                    h = r[n - 1].h;
                }
            } else {
                // Nothing before it, so the leading edge of the first
                // character instead.
                n = TextLayoutRangeRects(tl, e->text, 0, e->text.len, r, 32);
                if (n > 0) {
                    x = e->x + r[0].x;
                    y = e->y + r[0].y;
                    h = r[0].h;
                }
            }
            TextLayoutRelease(tl);
        }
    }
    // last_layout: where the caret ended up inside the run, which is what
    // scroll_to measures against on the next move.
    if (e->input) {
        e->input->caretX = x - e->x;
    }
    if (e->caretOutX) {
        *e->caretOutX = x;
    }
    if (e->caretOutY) {
        *e->caretOutY = y + h;
    }
    CanvasFillRect(ctx, x, y, e->caretW, h, e->caretColor);
}

void PaintEl(PaintCtx* ctx, El* e) {
    PaintElNode(ctx, e, true);
    // The overlays are the tree's second stacking layer, and saying so is
    // what lets a scene keep them apart from the tree without knowing that
    // there were two walks. See PaintCtx::paintLayer.
    ctx->paintLayer = 1;
    PaintOverlays(ctx, e);
    ctx->paintLayer = 0;
}

static void PaintElNodeInner(PaintCtx* ctx, El* e, bool skipOverlay);

// with_element_opacity: the opacity in force while this element and its
// children paint is the one around it times its own, and it goes back to what
// it was afterwards.
static void PaintElNode(PaintCtx* ctx, El* e, bool skipOverlay) {
    if (!e || !ctx) {
        return;
    }
    // `group("")`: what a descendant's group_hover asks about is the pointer
    // being in this box, which is not the same question as `hoverId` — the
    // close button drawn over a card takes the hover away from the card, and
    // Rust's group hitbox does not care.
    bool prevGroup = ctx->groupHovered;
    if (e->style.group) {
        ctx->groupHovered = e->w > 0 && e->h > 0 &&
                            e->Bounds().Contains({ctx->mouseX, ctx->mouseY});
    }
    if (e->style.opacity >= 1.f) {
        PaintElNodeInner(ctx, e, skipOverlay);
    } else {
        float prev = ctx->opacity;
        ctx->opacity = prev * e->style.opacity;
        PaintElNodeInner(ctx, e, skipOverlay);
        ctx->opacity = prev;
    }
    ctx->groupHovered = prevGroup;
}

static void PaintElNodeInner(PaintCtx* ctx, El* e, bool skipOverlay) {
    if (!e || !ctx->rt) {
        return;
    }
    if (skipOverlay && IsOverlay(e)) {
        return;
    }
    // `.invisible()` until the group is hovered. The box was laid out either
    // way; this only stops it being drawn.
    if (e->style.groupHoverVisible && !ctx->groupHovered) {
        return;
    }
    // SliderIndicator::on_prepaint. Layout is over by the time an element
    // paints, so its box is final and the slider can map a position onto it.
    if (e->sliderBounds) {
        SliderSetBounds(e->sliderBounds, e->Bounds());
    }
    if (e->boundsOut) {
        *e->boundsOut = e->Bounds();
    }
    // The inspector picking an element. GPUI offers the topmost *hitbox*
    // under the pointer; the nearest thing to a hitbox here is an element
    // that draws something or answers to an id, which is what keeps an
    // invisible layout container — or the full-window layer the overlays are
    // painted into, which goes down after everything else — from standing in
    // front of the button you aimed at. Among those, the deepest wins, and a
    // tie goes to the one painted later.
    int tier = e->clickId != 0 ? 2
                               : (e->style.hasBg || e->style.border > 0 ||
                                          e->kind != ElKind::Div
                                      ? 1
                                      : 0);
    bool better = !ctx->pickHit || tier > ctx->pickTier ||
                  (tier == ctx->pickTier && ctx->paintDepth >= ctx->pick.depth);
    if (ctx->picking && tier > 0 && better && e->w > 0 && e->h > 0 &&
        e->Bounds().Contains({ctx->mouseX, ctx->mouseY})) {
        InspectorPick p;
        p.id = e->clickId;
        p.elId = e->id;
        p.style = e->style;
        p.bounds = e->Bounds();
        p.kind = (int)e->kind;
        p.hasBg = e->style.hasBg;
        p.bg = e->style.bg.color;
        p.pad = e->style.pad.left;
        p.gap = e->style.gapX;
        p.radius = e->style.radius;
        p.border = e->style.border;
        p.row = e->style.dir == FlexDir::Row;
        p.font = e->style.fontSize;
        p.text = e->text;
        p.depth = ctx->paintDepth;
        ctx->pick = p;
        ctx->pickTier = tier;
        ctx->pickHit = true;
    }
    // What this element's children name as their ancestor: this element if it
    // recorded a hit rect, and whatever was around it if it did not.
    int outerHitParent = ctx->hitParent;
    if (e->clickId || e->onClick.IsValid() || e->listener.IsValid() ||
        e->clickAction || e->onHover.IsValid() || e->onMouseDown.IsValid() ||
        e->onMouseUp.IsValid() || e->onDragMove.IsValid() ||
        e->onMouseUpOut.IsValid() || e->drag.IsValid() || e->onDrop.IsValid() ||
        e->cursor != CursorKind::Arrow || e->slider) {
        HitRect hr;
        hr.id = e->clickId;
        hr.bounds = e->Bounds();
        hr.onClick = e->onClick;
        hr.clickAction = e->clickAction;
        hr.clickActionArg = e->clickActionArg;
        hr.listener = e->listener;
        hr.onHover = e->onHover;
        hr.tooltip = e->style.tooltip;
        hr.onMouseDown = e->onMouseDown;
        hr.onMouseUp = e->onMouseUp;
        hr.mouseDownPhase = e->mouseDownPhase;
        hr.mouseUpPhase = e->mouseUpPhase;
        hr.parent = ctx->hitParent;
        hr.onDragMove = e->onDragMove;
        hr.drag = e->drag;
        hr.onMouseUpOut = e->onMouseUpOut;
        hr.dropKind = e->dropKind;
        hr.onDrop = e->onDrop;
        hr.cursor = e->cursor;
        hr.slider = e->slider;
        hr.sliderAxis = e->sliderAxis;
        hr.input = e->input;
        ctx->hits.Append(hr);
        // Everything under this element names it as the ancestor its events
        // pass through, which is the chain the two phases walk.
        ctx->hitParent = ctx->hits.len - 1;
    }
    if (e->style.overflowY == Overflow::Scroll ||
        e->style.overflowX == Overflow::Scroll) {
        ScrollRect sr;
        sr.id = e->scrollId;
        sr.bounds = e->Bounds();
        sr.contentH = e->contentH;
        sr.scrollY = e->scrollY;
        sr.contentW = e->contentW;
        sr.scrollX = e->scrollX;
        sr.mode = ElScrollMode(e);
        sr.barX = !e->noScrollbar && !e->noScrollbarX;
        sr.barY = !e->noScrollbar && !e->noScrollbarY;
        sr.onScroll = e->onScroll;
        sr.input = e->input;
        ctx->scrolls.Append(sr);
    }

    // focus_ring_style: a focused control's own border takes the ring colour.
    // That is the half of the focus appearance that costs no room, and the
    // half Rust keeps when a theme turns the ring off. The element is this
    // frame's arena copy, so writing the colour onto it is what Rust's
    // `.border_color(cx.theme().ring)` does to the style it is building.
    // `.when(is_focused && self.focus_ring_enabled, ..)`: the control's own
    // opt-out drops the whole focus appearance, both halves of it.
    bool focused = e->style.focusId && e->style.focusId == ctx->focusId &&
                   e->style.focusRing;
    if (focused) {
        e->style.borderColor = ThemeNow().ring;
    }

    // The hover background needs a click id of its own: without one the
    // element would match hoverId 0, which means nothing is hovered.
    if (e->style.hasHoverBg && e->clickId && e->clickId == ctx->hoverId) {
        FillBackground(ctx, e->x, e->y, e->w, e->h, e->style.radius,
                       e->style.hasCorners ? &e->style.corners : nullptr,
                       e->style.hoverBg);
    } else if (e->style.hasBg) {
        FillBackground(ctx, e->x, e->y, e->w, e->h, e->style.radius,
                       e->style.hasCorners ? &e->style.corners : nullptr,
                       e->style.bg);
    }
    if (e->style.border > 0) {
        if (e->style.borderDashed) {
            // In stroke widths; the default is what GPUI's border_dashed
            // draws. D2D's own DASH style is 2/2 and reads too sparse.
            const float dash[2] = {e->style.dashOn, e->style.dashOff};
            float half = e->style.border * 0.5f;
            if (e->style.radius <= 0) {
                // Square corners: stroke each side on its own, so both the
                // line and the dashes along it can land on whole pixels.
                float l = EdgeLine(ctx, e->x + half);
                float r = EdgeLine(ctx, e->x + e->w - half);
                float t = EdgeLine(ctx, e->y + half);
                float b = EdgeLine(ctx, e->y + e->h - half);
                float x0 = EdgeEnd(ctx, e->x);
                float x1 = EdgeEnd(ctx, e->x + e->w);
                float y0 = EdgeEnd(ctx, e->y);
                float y1 = EdgeEnd(ctx, e->y + e->h);
                Rgba bc = e->style.borderColor;
                float bw = e->style.border;
                CanvasLine(ctx, x0, t, x1, t, bw, bc, dash);
                CanvasLine(ctx, x0, b, x1, b, bw, bc, dash);
                CanvasLine(ctx, l, y0, l, y1, bw, bc, dash);
                CanvasLine(ctx, r, y0, r, y1, bw, bc, dash);
            } else {
                CanvasStrokeRound(ctx, e->x, e->y, e->w, e->h, e->style.radius,
                                  e->style.border, e->style.borderColor, dash);
            }
        } else if (e->style.hasCorners) {
            StrokeCorners(ctx, e->x, e->y, e->w, e->h, e->style.corners,
                          e->style.border, e->style.borderColor);
        } else {
            DrawRoundStroke(ctx, e->x, e->y, e->w, e->h, e->style.radius,
                            e->style.border, e->style.borderColor);
        }
    }
    // An edge border sits inside the box and covers whole pixels: the line
    // goes half a stroke in from the edge, and lands on a device pixel.
    if (e->style.borderT > 0) {
        float y = EdgeLine(ctx, e->y + e->style.borderT * 0.5f);
        DrawLine(ctx, e->x, y, e->x + e->w, y, e->style.borderT,
                 e->style.borderColor);
    }
    if (e->style.borderB > 0) {
        float y = EdgeLine(ctx, e->y + e->h - e->style.borderB * 0.5f);
        DrawLine(ctx, e->x, y, e->x + e->w, y, e->style.borderB,
                 e->style.borderColor);
    }
    if (e->style.borderL > 0) {
        float x = EdgeLine(ctx, e->x + e->style.borderL * 0.5f);
        DrawLine(ctx, x, e->y, x, e->y + e->h, e->style.borderL,
                 e->style.borderColor);
    }
    if (e->style.borderR > 0) {
        float x = EdgeLine(ctx, e->x + e->w - e->style.borderR * 0.5f);
        DrawLine(ctx, x, e->y, x, e->y + e->h, e->style.borderR,
                 e->style.borderColor);
    }

    bool clip = e->style.overflowY != Overflow::Visible ||
                e->style.overflowX != Overflow::Visible;
    if (clip) {
        CanvasPushClip(ctx, e->x, e->y, e->w, e->h);
    }

    // InputElement's input_bounds: the box a press maps against. The
    // outermost binding of the frame wins, so the themed field's whole
    // bordered box counts and not just the run inside it.
    if (e->input && e->kind != ElKind::Text) {
        bool seen = false;
        for (int i = 0; i < ctx->inputs.len && !seen; i++) {
            seen = ctx->inputs[i] == e->input;
        }
        if (!seen) {
            e->input->inputBounds = e->Bounds();
            // The box the field scrolls inside, less what it pads by.
            e->input->viewW = e->w - e->style.pad.HorizontalAxisSum();
            e->input->viewH = e->h - e->style.pad.VerticalAxisSum();
            // scroll_size.width: how wide the longest row came out, which is
            // what a sideways scroll clamps against. Only a box that scrolls
            // that way reports it — a field that clips instead never moves
            // sideways, and a stale width would leave its caret arithmetic
            // scrolling text that cannot move.
            if (e->style.overflowX == Overflow::Scroll) {
                e->input->contentW = e->contentW;
            }
            ctx->inputs.Append(e->input);
        }
    }
    if (e->kind == ElKind::Text) {
        float font = e->laidFont > 0
                         ? e->laidFont
                         : (e->style.fontSize > 0 ? e->style.fontSize : 14.f);
        if (e->input) {
            e->input->lastBounds = e->Bounds();
            e->input->lastFont = font;
        }
        Rgba c = e->style.hasColor ? e->style.color : ThemeNow().foreground;
        int lo = e->selLo;
        int hi = e->selHi;
        if (e->selectable && e->text.s) {
            int docOff = ctx->textDocLen;
            TextHit th;
            th.bounds = e->Bounds();
            th.text = e->text;
            th.font = font;
            th.maxW = e->laidMaxW > 0 ? e->laidMaxW : e->w;
            th.wrap = e->style.wrap;
            th.docOff = docOff;
            // The trap this run sits in — a dialog, a sheet — which is the
            // TextSelectionScopeId a gesture inside it stays within.
            th.scope = e->style.trapId;
            ctx->texts.Append(th);
            ctx->textDocLen += e->text.len + 1;
            int a = ctx->selA;
            int b = ctx->selB;
            if (ctx->selScope >= 0 && ctx->selScope != e->style.trapId) {
                a = -1;
                b = -1;
            }
            if (a >= 0 && b >= 0 && a != b) {
                if (a > b) {
                    int t = a;
                    a = b;
                    b = t;
                }
                int tlo = a > docOff ? a : docOff;
                int thi = b < docOff + e->text.len ? b : docOff + e->text.len;
                if (tlo < thi) {
                    lo = tlo - docOff;
                    hi = thi - docOff;
                }
            }
        }
        // truncate: a run that does not wrap is the same size whatever width
        // it was measured against, so the shaped run is cached without one and
        // the box it was drawn for cannot do the cutting. This can.
        bool clipText = e->style.truncate && e->laidMaxW > 0;
        if (clipText) {
            CanvasPushClip(ctx, e->x, e->y, e->laidMaxW, e->h);
        }
        // Under the selection quad as well as under the glyphs: a match the
        // caret happens to be inside still reads as selected.
        for (int i = 0; i < e->nWashes; i++) {
            const TextSpan& w = e->washes[i];
            if (w.bg.a == 0 || w.hi <= w.lo) {
                continue;
            }
            PaintTextRange(ctx, e->text, font,
                           e->laidMaxW > 0 ? e->laidMaxW : e->w, e->style.wrap,
                           ElTextWeight(e), e->style.lineHeight, e->x, e->y,
                           w.lo, w.hi, w.bg);
        }
        if (lo >= 0 && hi > lo) {
            PaintTextRange(ctx, e->text, font,
                           e->laidMaxW > 0 ? e->laidMaxW : e->w, e->style.wrap,
                           ElTextWeight(e), e->style.lineHeight, e->x, e->y, lo,
                           hi, e->selColor);
        }
        if (e->markLo >= 0 && e->markHi > e->markLo) {
            PaintTextUnderline(
                ctx, e->text, font, e->laidMaxW > 0 ? e->laidMaxW : e->w,
                e->style.wrap, e->x, e->y, e->markLo, e->markHi, c);
        }
        if (e->nSpans > 0 && e->text.s) {
            PaintTextSpans(ctx, e, font, c);
        } else if (e->laidLayout) {
            TextLayoutDraw(ctx, e->laidLayout, e->x, e->y, c, e->style.truncate,
                           e->laidMaxW);
        } else {
            DrawTextAt(ctx, e->text, e->x, e->y, e->w, e->h, font, c,
                       e->style.truncate, e->style.wrap, e->laidMaxW,
                       ElTextWeight(e), e->style.lineHeight);
        }
        // range_to_bounds: where a named run of this text landed, for a
        // caller that hit-tests against it on a later frame.
        if (e->rangeOut && e->rangeOutHi > e->rangeOutLo) {
            *e->rangeOut = Bounds{};
            TextLayout* tl = TextMeasLayout(
                ctx, e->text, font, e->laidMaxW > 0 ? e->laidMaxW : e->w,
                e->style.wrap, (uint8_t)ElTextWeight(e), e->style.lineHeight,
                nullptr);
            if (tl) {
                Bounds r[8] = {};
                int n = TextLayoutRangeRects(tl, e->text, e->rangeOutLo,
                                             e->rangeOutHi, r, 8);
                if (n > 0) {
                    *e->rangeOut = {e->x + r[0].x, e->y + r[0].y, r[0].w,
                                    r[0].h};
                }
                TextLayoutRelease(tl);
            }
        }
        // The rules a diagnostic asked for, over whatever drew the glyphs.
        for (int i = 0; i < e->nUnderlines; i++) {
            const TextSpan& u = e->underlines[i];
            if (u.hi <= u.lo || u.color.a == 0) {
                continue;
            }
            PaintTextUnderline(
                ctx, e->text, font, e->laidMaxW > 0 ? e->laidMaxW : e->w,
                e->style.wrap, e->x, e->y, u.lo, u.hi, u.color, u.wavy);
        }
        if (clipText) {
            CanvasPopClip(ctx);
        }
        PaintCaret(ctx, e, font);
    } else if (e->kind == ElKind::Image) {
        // image.h resolves the src: the asset an application shipped, the
        // data: URI, or the body a worker thread fetched. A fetch still
        // running answers nothing, and the alt text below stands in until it
        // lands.
        Image* img = ImageForSrc(ctx->pa, e->imgSrc);
        int opsLen = 0;
        const uint8_t* ops =
            img ? nullptr : ImageVectorForSrc(e->imgSrc, &opsLen);
        if (img) {
            ImageDraw(ctx, img, e->Bounds(), e->style.radius);
        } else if (SvgDrawOps(ctx, ops, opsLen, e->x, e->y, e->w, e->h,
                              e->style.hasColor ? e->style.color
                                                : ThemeNow().foreground,
                              0)) {
            // An SVG is not a bitmap for any of the three backends to decode;
            // it is the vector the icon renderer already walks, and a picture
            // with colours of its own keeps them. Into the whole box, not a
            // square inside it: an image element is laid out at the picture's
            // own aspect, so the box is already the shape to draw into.
        } else if (e->text.s && e->text.len > 0) {
            // The alt text, in the color the text around it uses.
            float font =
                e->laidFont > 0
                    ? e->laidFont
                    : (e->style.fontSize > 0 ? e->style.fontSize : 14.f);
            Rgba c = e->style.hasColor ? e->style.color : ThemeNow().mutedFg;
            DrawTextAt(ctx, e->text, e->x, e->y, e->w, e->h, font, c, false,
                       e->style.wrap, e->laidMaxW, ElTextWeight(e),
                       e->style.lineHeight);
        }
    } else if (e->kind == ElKind::Icon) {
        Rgba c = e->style.hasColor ? e->style.color : ThemeNow().foreground;
        float s = e->w > 0 ? e->w : 16;
        // Every lucide icon is compiled in as draw-op bytecode
        // (asset_icons.cpp), so this reads no file; an application's own
        // `.svg` is converted to the same bytecode the first time it is
        // asked for.
        Str path = e->iconPath.s ? e->iconPath : IconNamePath(e->icon);
        SvgDraw(ctx, path, e->x, e->y, s, c, e->style.rotate);
    } else if (e->kind == ElKind::Progress) {
        const Theme& th = ThemeNow();
        Background track = BackgroundOpacity(th.tokens.progress, 0.2f);
        FillBackground(ctx, e->x, e->y, e->w, e->h, e->style.radius, nullptr,
                       track);
        float fw = e->w * (e->progress / 100.f);
        if (fw > 0) {
            FillBackground(ctx, e->x, e->y, fw, e->h, e->style.radius, nullptr,
                           th.tokens.progress);
        }
    } else if (e->kind == ElKind::Chart) {
        DrawChart(ctx, e);
    }
    if (e->kind != ElKind::Text) {
        PaintCaret(ctx, e, e->laidFont > 0 ? e->laidFont : 14.f);
    }
    if (e->customPaint) {
        e->customPaint(ctx, e, e->customUser);
    }

    ctx->paintDepth++;
    for (El* c = e->first; c; c = c->next) {
        PaintElNode(ctx, c, skipOverlay);
    }
    ctx->hitParent = outerHitParent;
    ctx->paintDepth--;

    if (clip) {
        CanvasPopClip(ctx);
    }

    // ScrollbarMode: Always paints the bar whenever there is something to
    // scroll, Hover only while the pointer is over the box it belongs to, and
    // Scrolling while the offset is moving plus the fade after it stops.
    ScrollbarMode barMode = ElScrollMode(e);
    bool overBox = e->Bounds().Contains({ctx->mouseX, ctx->mouseY});
    bool barVisible =
        !e->noScrollbar && (barMode == ScrollbarMode::Always || overBox);
    float barAlpha = 1.f;
    if (!e->noScrollbar && barMode == ScrollbarMode::Scrolling) {
        // No ScrollId is no clock: the area cannot be found again next frame,
        // so it keeps the bar rather than blinking it every time the pointer
        // moves. Rust's state is keyed the same way, off the element id.
        if (e->scrollId == 0) {
            barVisible = true;
        } else {
            // is_hovered_on_bar: the pointer resting inside the band the
            // thumb runs down holds the bar up, which is Rust stamping the
            // time again on every frame it is there.
            bool held =
                overBox && (ctx->mouseX >= e->x + e->w - kScrollbarBandW ||
                            ctx->mouseY >= e->y + e->h - kScrollbarBandW);
            barAlpha = ScrollFadeOpacity(e->scrollId, e->scrollY, e->scrollX,
                                         held, &ctx->wantsAnimFrame);
            barVisible = barAlpha > 0.f;
        }
    }
    // style_for_normal / style_for_hovered_bar / style_for_hovered_thumb /
    // style_for_active. A bar rests at THUMB_WIDTH only in the fading
    // `Scrolling` mode; every other mode draws the wide one, and any bar the
    // pointer is over — or one a drag has hold of — grows to it too. The
    // colour changes only under the thumb itself, or in a drag.
    bool dragging = ctx->scrollDragId != 0 && ctx->scrollDragId == e->scrollId;
    float restW = barMode == ScrollbarMode::Scrolling ? kScrollbarThumbW
                                                      : kScrollbarThumbActiveW;
    // The thumb's radius is `theme.radius`, clamped to half the thumb — a
    // wider thumb rounds more, which is what `clamp_thumb_radius` says.
    const Theme& barTheme = ThemeNow();
    if (barVisible && !e->noScrollbarY &&
        e->style.overflowY == Overflow::Scroll && e->contentH > e->h + 1.f &&
        e->h > 0) {
        bool onBar = overBox && ctx->mouseX >= e->x + e->w - kScrollbarBandW;
        bool hot = onBar || (dragging && !ctx->scrollDragHorizontal);
        // The same three numbers the press and drag arithmetic goes by, so
        // what is drawn and what is grabbed cannot drift apart.
        float thumbH = ScrollbarThumbSize(e->h, e->h, e->contentH);
        float thumbW = hot ? kScrollbarThumbActiveW : restW;
        float thumbX = e->x + e->w - thumbW - kScrollbarThumbMargin;
        float thumbY = e->y + ScrollbarThumbPos(e->h, thumbH, e->scrollY, e->h,
                                                e->contentH);
        bool onThumb =
            (dragging && !ctx->scrollDragHorizontal) ||
            (onBar && ctx->mouseY >= thumbY && ctx->mouseY < thumbY + thumbH);
        // The track, which every default theme leaves transparent — the band
        // is Rust's WIDTH and reaches the whole length of the box.
        FillBackground(ctx, e->x + e->w - kScrollbarBandW, e->y,
                       kScrollbarBandW, e->h, 0, nullptr,
                       ScrollbarBarBg(barTheme, barAlpha));
        FillBackground(ctx, thumbX, thumbY, thumbW, thumbH,
                       ThumbRadius(barTheme, thumbW), nullptr,
                       ScrollbarThumbBg(barTheme, onThumb, barAlpha));
    }
    if (barVisible && !e->noScrollbarX &&
        e->style.overflowX == Overflow::Scroll && e->contentW > e->w + 1.f &&
        e->w > 0) {
        // The horizontal bar is the same arithmetic along the other axis,
        // which is how Rust writes it: one path, `is_vertical` picking the
        // pair of numbers it reads.
        bool onBar = overBox && ctx->mouseY >= e->y + e->h - kScrollbarBandW;
        bool hot = onBar || (dragging && ctx->scrollDragHorizontal);
        float thumbW = ScrollbarThumbSize(e->w, e->w, e->contentW);
        float thumbH = hot ? kScrollbarThumbActiveW : restW;
        float thumbY = e->y + e->h - thumbH - kScrollbarThumbMargin;
        float thumbX = e->x + ScrollbarThumbPos(e->w, thumbW, e->scrollX, e->w,
                                                e->contentW);
        bool onThumb =
            (dragging && ctx->scrollDragHorizontal) ||
            (onBar && ctx->mouseX >= thumbX && ctx->mouseX < thumbX + thumbW);
        FillBackground(ctx, e->x, e->y + e->h - kScrollbarBandW, e->w,
                       kScrollbarBandW, 0, nullptr,
                       ScrollbarBarBg(barTheme, barAlpha));
        FillBackground(ctx, thumbX, thumbY, thumbW, thumbH,
                       ThumbRadius(barTheme, thumbH), nullptr,
                       ScrollbarThumbBg(barTheme, onThumb, barAlpha));
    }

    if (focused && ThemeFocusRing()) {
        // The other half of focus_ring_style: FOCUS_RING_WIDTH of the ring
        // colour at FOCUS_RING_OPACITY, in the three DIPs immediately outside
        // the element's border, with the corners widened to match. Rust hangs
        // it off `is_focused` alone — a control focused by a click shows it as
        // much as one reached with Tab — and paints it as an absolutely
        // placed child, which an ancestor that clips will cut off either way.
        // Which is why `Theme::focus_ring` exists: an application that clips
        // its containers turns the ring off and keeps the border above.
        Bounds ring = e->Bounds().Inset(-kFocusRingWidth);
        DrawRoundStroke(ctx, ring.x, ring.y, ring.w, ring.h,
                        e->style.radius + kFocusRingWidth, kFocusRingWidth,
                        RgbaOpacity(ThemeNow().ring, kFocusRingOpacity));
    }
}

// TooltipOverlay::render. The overlay says what is showing and where its
// trigger was, so the tip outlives the element that asked for it — which it
// has to, since the countdown that revealed it lands frames later.
void TooltipPaint(PaintCtx* ctx, const TooltipOverlay* tip) {
    if (!tip || !tip->visible || !tip->text.s) {
        return;
    }
    const Theme& th = ThemeNow();
    // tooltip.rs: the popover surface with the theme border round it, not a
    // dark plate — `bg(tokens.popover).text_color(popover_foreground)
    // .border_1().border_color(border).rounded(6).py_0p5().px_2().text_sm()`.
    // The port painted it in `foreground` on `background`, which reads as an
    // inverted chip in either theme.
    const float kPadX = 8.f, kPadY = 2.f, kBorder = 1.f;
    Size sz = MeasureText(ctx, tip->text, 14, 280);
    // TooltipPositioner: the shared positioner's side placement, with no
    // preferred side (which prefers above), centered on the trigger, the
    // window margin, and `m_3` of clearance from the trigger itself.
    Positioned at = PositionSide(
        tip->triggerBounds,
        {sz.w + kPadX * 2 + kBorder * 2, sz.h + kPadY * 2 + kBorder * 2},
        {ctx->viewW, ctx->viewH}, kPopupMargin, nullptr, PopupAlign::Center,
        10.f);
    FillRound(ctx, at.bounds.x, at.bounds.y, at.bounds.w, at.bounds.h, 6,
              th.background);
    DrawRoundStroke(ctx, at.bounds.x, at.bounds.y, at.bounds.w, at.bounds.h, 6,
                    kBorder, th.border);
    DrawTextAt(ctx, tip->text, at.bounds.x + kPadX + kBorder,
               at.bounds.y + kPadY + kBorder, sz.w + 4, sz.h, 14, th.foreground,
               false);
}

const HitRect* HitTestRect(PaintCtx* ctx, float x, float y) {
    for (int i = ctx->hits.len - 1; i >= 0; i--) {
        const HitRect& h = ctx->hits[i];
        if (h.bounds.Contains({x, y})) {
            return &ctx->hits[i];
        }
    }
    return nullptr;
}

const HitRect* HitTestDrop(PaintCtx* ctx, float x, float y, Str kind) {
    if (kind.s == nullptr) {
        return nullptr;
    }
    for (int i = ctx->hits.len - 1; i >= 0; i--) {
        const HitRect& h = ctx->hits[i];
        if (!h.onDrop.IsValid() || h.dropKind.s == nullptr) {
            continue;
        }
        if (!StrSame(h.dropKind, kind)) {
            continue;
        }
        if (h.bounds.Contains({x, y})) {
            return &ctx->hits[i];
        }
    }
    return nullptr;
}

InputState* InputAtPosition(PaintCtx* ctx, float x, float y) {
    for (int i = ctx->inputs.len - 1; i >= 0; i--) {
        InputState* s = ctx->inputs[i];
        if (s->inputBounds.Contains({x, y})) {
            return s;
        }
    }
    return nullptr;
}

int HitTest(PaintCtx* ctx, float x, float y) {
    const HitRect* h = HitTestRect(ctx, x, y);
    return h ? h->id : 0;
}

const ScrollRect* HitScrollRect(PaintCtx* ctx, float x, float y) {
    for (int i = ctx->scrolls.len - 1; i >= 0; i--) {
        const ScrollRect& s = ctx->scrolls[i];
        if (s.bounds.Contains({x, y})) {
            return &ctx->scrolls[i];
        }
    }
    return nullptr;
}

static float DistToInterval(float v, float lo, float hi) {
    if (v < lo) {
        return lo - v;
    }
    if (v > hi) {
        return v - hi;
    }
    return 0.f;
}

// The selectable run under (x, y), plus where inside it the point landed.
// `nearest` widens the search to the closest run when none contains the point,
// which is what a drag past the end of a paragraph needs.
// `scope` is a TextSelectionScopeId — the trap the run sits in — and -1 is
// every one of them. A drag that began inside a dialog cannot reach the page
// behind it, which is what Rust's activate_scope arranges.
static const TextHit* TextHitFind(PaintCtx* ctx, float x, float y, bool nearest,
                                  Point* outRel, int scope = -1) {
    if (!ctx) {
        return nullptr;
    }
    const TextHit* best = nullptr;
    float bestScore = 1e9f;
    for (int i = ctx->texts.len - 1; i >= 0; i--) {
        const TextHit& h = ctx->texts[i];
        if (scope >= 0 && h.scope != scope) {
            continue;
        }
        if (h.bounds.Contains({x, y})) {
            best = &h;
            nearest = false;
            break;
        }
        if (!nearest) {
            continue;
        }
        float dy = DistToInterval(y, h.bounds.y, h.bounds.Bottom());
        float dx = DistToInterval(x, h.bounds.x, h.bounds.Right());
        float score = dy * 1000.f + dx;
        if (score < bestScore) {
            bestScore = score;
            best = &h;
        }
    }
    if (!best || !best->text.s) {
        return nullptr;
    }
    Point rel = {x - best->bounds.x, y - best->bounds.y};
    if (nearest) {
        if (rel.x < 0) {
            rel.x = 0;
        }
        if (rel.y < 0) {
            rel.y = 0;
        }
        if (rel.x > best->bounds.w) {
            rel.x = best->bounds.w;
        }
        if (rel.y > best->bounds.h) {
            rel.y = best->bounds.h;
        }
    }
    *outRel = rel;
    return best;
}

// The byte offset inside `h` that `rel` points at, clamped into it.
static int TextHitLocal(PaintCtx* ctx, const TextHit* h, Point rel) {
    int local =
        TextIndexAt(ctx, h->text, h->font, h->maxW > 0 ? h->maxW : h->bounds.w,
                    h->wrap, rel.x, rel.y);
    if (local < 0) {
        local = 0;
    }
    if (local > h->text.len) {
        local = h->text.len;
    }
    return local;
}

int TextHitOffsetAt(PaintCtx* ctx, float x, float y, bool nearest) {
    return TextHitOffsetIn(ctx, x, y, nearest, -1, nullptr);
}

int TextHitOffsetIn(PaintCtx* ctx, float x, float y, bool nearest, int scope,
                    int* outScope) {
    Point rel = {};
    const TextHit* h = TextHitFind(ctx, x, y, nearest, &rel, scope);
    if (!h) {
        return -1;
    }
    if (outScope) {
        *outScope = h->scope;
    }
    return h->docOff + TextHitLocal(ctx, h, rel);
}

// The character at `i` and how many bytes it took. A byte that is not valid
// UTF-8 counts as one character of its own value: the rules above only ask
// which class it lands in, and every stray byte lands in the same one.
int Utf8At(Str s, int i, uint32_t* out) {
    const uint8_t* p = (const uint8_t*)s.s + i;
    uint8_t c = p[0];
    if (c < 0x80) {
        *out = c;
        return 1;
    }
    int n = (c & 0xE0) == 0xC0   ? 2
            : (c & 0xF0) == 0xE0 ? 3
            : (c & 0xF8) == 0xF0 ? 4
                                 : 1;
    if (n == 1 || i + n > s.len) {
        *out = c;
        return 1;
    }
    uint32_t cp = (uint32_t)(c & (0xFF >> (n + 1)));
    for (int k = 1; k < n; k++) {
        if ((p[k] & 0xC0) != 0x80) {
            *out = c;
            return 1;
        }
        cp = (cp << 6) | (uint32_t)(p[k] & 0x3F);
    }
    *out = cp;
    return n;
}

// Where the character before `i` starts.
int Utf8Prev(Str s, int i) {
    int j = i - 1;
    while (j > 0 && ((uint8_t)s.s[j] & 0xC0) == 0x80) {
        j--;
    }
    return j < 0 ? 0 : j;
}

bool TextMultiClickRange(PaintCtx* ctx, float x, float y, int clickCount,
                         int* outA, int* outB) {
    return TextMultiClickRangeIn(ctx, x, y, clickCount, -1, outA, outB,
                                 nullptr);
}

bool TextMultiClickRangeIn(PaintCtx* ctx, float x, float y, int clickCount,
                           int scope, int* outA, int* outB, int* outScope) {
    if (clickCount < 2) {
        return false;
    }
    Point rel = {};
    const TextHit* h = TextHitFind(ctx, x, y, false, &rel, scope);
    if (!h) {
        return false;
    }
    int local = TextHitLocal(ctx, h, rel);
    int a = 0;
    int b = 0;
    if (clickCount == 2) {
        if (!TextWordRangeAt(h->text, local, &a, &b)) {
            return false;
        }
    } else {
        TextLineRangeAt(h->text, local, &a, &b);
    }
    if (a >= b) {
        return false;
    }
    if (outScope) {
        *outScope = h->scope;
    }
    *outA = h->docOff + a;
    *outB = h->docOff + b;
    return true;
}

int CopyTextHits(PaintCtx* ctx, int a, int b, char* out, int cap) {
    return CopyTextHitsIn(ctx, a, b, -1, out, cap);
}

int CopyTextHitsIn(PaintCtx* ctx, int a, int b, int scope, char* out, int cap) {
    if (!out || cap <= 0) {
        return 0;
    }
    out[0] = 0;
    if (!ctx || a < 0 || b < 0 || a == b) {
        return 0;
    }
    if (a > b) {
        int t = a;
        a = b;
        b = t;
    }
    int n = 0;
    for (int i = 0; i < ctx->texts.len && n < cap - 1; i++) {
        const TextHit& t = ctx->texts[i];
        if (scope >= 0 && t.scope != scope) {
            continue;
        }
        int pos = t.docOff;
        int plen = t.text.len;
        int lo = a > pos ? a : pos;
        int hi = b < pos + plen ? b : pos + plen;
        if (lo < hi && t.text.s) {
            int take = hi - lo;
            if (n + take > cap - 1) {
                take = cap - 1 - n;
            }
            memcpy(out + n, t.text.s + (lo - pos), (size_t)take);
            n += take;
        }
        int gap = pos + plen;
        if (i + 1 < ctx->texts.len && a <= gap && b > gap && n < cap - 1) {
            out[n++] = '\n';
        }
    }
    out[n] = 0;
    return n;
}

// A trap is a property of the container, the way Rust hangs it off the one
// focus handle the dialog tracks, so it reaches every focusable below it. The
// resolved id is written back onto the element: the focus ring paints from it,
// and nothing else has the tree to work it out again.
static void CollectFocus(El* e, Window* win, int trap) {
    if (!e) {
        return;
    }
    if (e->style.trapId) {
        trap = e->style.trapId;
    }
    // The context comes before this element's own handlers, so a walk out
    // from here finds the handlers first and then the context they sit in,
    // which is the order Rust reads them.
    int first = win->dispatch.len;
    if (e->style.keyContext) {
        DispatchNode n;
        n.context = e->style.keyContext;
        win->dispatch.Append(n);
    }
    for (ActionSlot* slot = e->actions; slot; slot = slot->next) {
        DispatchNode n;
        n.action = slot->action;
        n.fn = slot->fn;
        win->dispatch.Append(n);
    }
    if (e->style.focusId) {
        e->style.trapId = trap;
        FocusRect fr;
        fr.id = e->style.focusId;
        fr.trapId = trap;
        fr.tabIndex = e->style.tabIndex;
        fr.tabStop = e->style.tabStop;
        fr.focusOnPress = e->style.focusOnPress;
        // A marker of its own, so the element has a position inside its own
        // subtree whether or not it declared a context or a handler. Without
        // one, an element that declares neither would share an index with the
        // end of the subtree beside it and pick up that sibling's context.
        DispatchNode marker;
        fr.dispatchIx = win->dispatch.len;
        win->dispatch.Append(marker);
        fr.bounds = e->Bounds();
        win->focusEls.Append(fr);
    }
    for (El* c = e->first; c; c = c->next) {
        CollectFocus(c, win, trap);
    }
    // The subtree is closed: everything from here down was written between
    // `first` and now, so anything focused in it sits inside this span.
    for (int i = first; i < win->dispatch.len; i++) {
        if (win->dispatch[i].subtreeEnd == 0) {
            win->dispatch[i].subtreeEnd = win->dispatch.len;
        }
    }
}

// --- action dispatch ------------------------------------------------------
//
// Rust walks the focused handle's ancestry twice: once up, gathering the key
// contexts a keystroke is matched against, and once down again, offering the
// action to each `on_action` until one keeps it. The tree here is gone by the
// time a key arrives, so `win->dispatch` is that walk recorded in tree order
// with a depth on every node: the ancestors of a node are the ones before it
// with a strictly smaller depth, taken smallest-so-far first.

// cx.on_action's table. Small and fixed: these are the framework's own
// handlers, not an application's, which hangs its own off its elements.
struct AppAction {
    uint32_t action = 0;
    ActionFn fn = nullptr;
};

static const int kMaxAppActions = 32;
static AppAction gAppActions[kMaxAppActions];
static int gNAppActions = 0;

void AppOnAction(uint32_t action, ActionFn fn) {
    if (!action || !fn || gNAppActions >= kMaxAppActions) {
        return;
    }
    gAppActions[gNAppActions].action = action;
    gAppActions[gNAppActions].fn = fn;
    gNAppActions++;
}

// inspector::init. Rust registers this from `gpui_component::init(cx)`, which
// an application calls once; there is no such call here, so the framework's
// own bindings go in the first time a keystroke looks for one.
static void ToggleInspectorAction(Window* win, ActionEvent*) {
    WindowToggleInspector(win);
}

static void KeymapDefaults() {
    static uint32_t done = 0;
    if (done == KeymapGeneration()) {
        return;
    }
    done = KeymapGeneration();
    uint32_t toggle = ActionOf(StrL("inspector::ToggleInspector"));
    KeyBinding bindings[] = {
#ifdef __APPLE__
        {"cmd-alt-i", toggle, nullptr},
#else
        {"ctrl-shift-i", toggle, nullptr},
#endif
    };
    KeymapBind(bindings, (int)(sizeof(bindings) / sizeof(bindings[0])));
    AppOnAction(toggle, &ToggleInspectorAction);
}

// Where in the dispatch list the focused element sits. Nothing focused is
// the end of the list, which leaves the root's chain — every node whose
// subtree is still open there, which is the window itself.
static int DispatchAnchor(Window* win) {
    if (win->focusId) {
        for (int i = 0; i < win->focusEls.len; i++) {
            if (win->focusEls[i].id == win->focusId) {
                return win->focusEls[i].dispatchIx;
            }
        }
    }
    return win->dispatch.len;
}

// The reserved action a raw key listener is recorded under. No chord resolves
// to it, so the keymap never reaches these; only WindowDispatchKeyEvent does.
static uint32_t KeyDownAction() {
    return ActionOf(StrL("gpui::KeyDown"));
}

bool WindowDispatchKeyEvent(Window* win, KeyEvent* ev) {
    if (!win || !ev) {
        return false;
    }
    uint32_t action = KeyDownAction();
    int ix = DispatchAnchor(win);
    for (int i = ix - 1; i >= 0; i--) {
        if (win->dispatch[i].subtreeEnd <= ix ||
            win->dispatch[i].action != action ||
            !win->dispatch[i].fn.IsValid()) {
            continue;
        }
        ev->propagate = true;
        ListenerCall(win->app, win, win->dispatch[i].fn, ev);
        if (!ev->propagate) {
            return true;
        }
    }
    return false;
}

uint32_t WindowResolveKeyAction(Window* win, int vk, bool shift, bool ctrl,
                                bool alt, bool platform, intptr_t* arg,
                                bool* pending) {
    if (arg) {
        *arg = 0;
    }
    if (pending) {
        *pending = false;
    }
    if (!win || !vk) {
        return 0;
    }
    int ix = DispatchAnchor(win);

    // The contexts stacked over the focused element, innermost first; the
    // keymap reads as deep a stack as kMaxContextDepth.
    uint32_t contexts[kMaxContextDepth];
    int nContexts = 0;
    for (int i = ix - 1; i >= 0 && nContexts < kMaxContextDepth; i--) {
        if (win->dispatch[i].subtreeEnd <= ix || !win->dispatch[i].context) {
            continue;
        }
        contexts[nContexts++] = win->dispatch[i].context;
    }

    KeymapDefaults();
    KeyChord chord;
    chord.vk = vk;
    chord.shift = shift;
    chord.ctrl = ctrl;
    chord.alt = alt;
    chord.platform = platform;
    KeyMatch m = KeymapMatch(chord, contexts, nContexts);
    if (m.pending) {
        // Half of a sequence. Rust holds the keystroke on the matcher and
        // dispatches nothing; here that is "eaten", so nothing under the
        // keymap sees it either.
        if (pending) {
            *pending = true;
        }
        return 0;
    }
    if (arg) {
        *arg = m.arg;
    }
    return m.action;
}

bool WindowDispatchKeyAction(Window* win, int vk, bool shift, bool ctrl,
                             bool alt, bool platform) {
    intptr_t arg = 0;
    bool pending = false;
    uint32_t action = WindowResolveKeyAction(win, vk, shift, ctrl, alt,
                                             platform, &arg, &pending);
    if (pending) {
        return true;
    }
    if (!action) {
        return false;
    }
    return WindowDispatchAction(win, action, arg);
}

// The handler half on its own: the chain over the focused element, then the
// application's. Rust's `window.dispatch_action(Box::new(Cancel), cx)` — a
// button that runs the same thing the escape key does, without a keystroke to
// resolve first.
bool WindowDispatchAction(Window* win, uint32_t action, intptr_t arg) {
    if (!win || !action) {
        return false;
    }
    int ix = DispatchAnchor(win);
    // A handler that propagates lets the search carry on outwards.
    for (int i = ix - 1; i >= 0; i--) {
        if (win->dispatch[i].subtreeEnd <= ix ||
            win->dispatch[i].action != action ||
            !win->dispatch[i].fn.IsValid()) {
            continue;
        }
        ActionEvent ev;
        ev.action = action;
        ev.arg = arg;
        ListenerCall(win->app, win, win->dispatch[i].fn, &ev);
        if (!ev.propagate) {
            return true;
        }
    }
    // Then the application's own, which is where the framework keeps the
    // handlers that belong to no element.
    for (int i = gNAppActions - 1; i >= 0; i--) {
        if (gAppActions[i].action != action) {
            continue;
        }
        ActionEvent ev;
        ev.action = action;
        ev.arg = arg;
        gAppActions[i].fn(win, &ev);
        if (!ev.propagate) {
            return true;
        }
    }
    // Bound but unhandled. Rust leaves the keystroke to whatever is under the
    // action dispatch, and so does this: the caller carries on.
    return false;
}

void FocusCollect(Window* win, El* root) {
    win->focusEls.Clear();
    win->dispatch.Clear();
    CollectFocus(root, win, 0);
    // The traversal order is the tab index first and the paint order within
    // it, so the sort has to be a stable one: an insertion sort over a list
    // this size, where almost every element is already index zero and nothing
    // moves at all.
    for (int i = 1; i < win->focusEls.len; i++) {
        FocusRect fr = win->focusEls[i];
        int j = i - 1;
        while (j >= 0 && win->focusEls[j].tabIndex > fr.tabIndex) {
            win->focusEls[j + 1] = win->focusEls[j];
            j--;
        }
        win->focusEls[j + 1] = fr;
    }
}

void WindowSetFocusId(Window* win, int id) {
    if (!win || win->focusId == id) {
        return;
    }
    win->focusId = id;
    // focus_generation: what a pending keystroke is stamped with, so the move
    // is what tells it the element under it changed.
    win->focusGen++;
}

int WindowFocusedId(const Window* win) {
    return win ? win->focusId : 0;
}

bool WindowFocusWithin(const Window* win, int id) {
    if (!win || !id) {
        return false;
    }
    if (win->focusId == id) {
        return true;
    }
    for (int i = 0; i < win->focusEls.len; i++) {
        if (win->focusEls[i].id == win->focusId) {
            return win->focusEls[i].trapId == id;
        }
    }
    return false;
}

bool WindowRestoreFocus(Window* win, int id) {
    if (!win || !id) {
        return false;
    }
    for (int i = 0; i < win->focusEls.len; i++) {
        if (win->focusEls[i].id == id) {
            WindowSetFocusId(win, id);
            return true;
        }
    }
    return false;
}

int FocusNext(Window* win, int trapId, bool backward) {
    int n = win->focusEls.len;
    if (n == 0) {
        return 0;
    }
    int cur = -1;
    for (int i = 0; i < n; i++) {
        if (win->focusEls[i].id == win->focusId) {
            cur = i;
            break;
        }
    }
    int step = backward ? -1 : 1;
    int i = cur;
    for (int k = 0; k < n; k++) {
        i = (i + step + n) % n;
        if (!win->focusEls[i].tabStop) {
            // Focusable, but not somewhere Tab stops.
            continue;
        }
        if (trapId && win->focusEls[i].trapId != trapId) {
            continue;
        }
        if (!trapId && win->focusEls[i].trapId) {
            // stay out of traps unless already inside
            if (cur < 0 || win->focusEls[cur].trapId == 0) {
                continue;
            }
            if (win->focusEls[i].trapId != win->focusEls[cur].trapId) {
                continue;
            }
        }
        WindowSetFocusId(win, win->focusEls[i].id);
        return win->focusId;
    }
    return win->focusId;
}
} // namespace gpui
