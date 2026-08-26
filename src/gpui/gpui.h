/* C++ GPUI subset used by system_monitor. Frame-rebuilt element tree. */

#include "base.h"
#include "taffy/taffy_tree.h"

// The base lives in `namespace base` so that `src/taffy` and `src/markdown` —
// ports of crates that have never heard of gpui — can be written against it
// and nothing else. gpui is the one module that treats it as its own
// vocabulary, so it takes the whole namespace in: `Str`, `Vec`, `Arena`,
// `fmt` and the rest are spelled unqualified below, and qualified lookup
// still finds them, so `gpui::Str` outside names what it always did.
namespace gpui {
using namespace base;
}

// ─── color ────────────────────────────────────────────────────────────────

namespace gpui {

struct Rgba {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

inline Rgba Rgb(uint8_t r, uint8_t g, uint8_t b) {
    return Rgba{r, g, b, 255};
}
inline Rgba Rgba8(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return Rgba{r, g, b, a};
}
inline Rgba RgbaHex(uint32_t hex) {
    // 0xRRGGBB or 0xAARRGGBB if top byte set
    if (hex > 0xFFFFFFu) {
        return Rgba{(uint8_t)((hex >> 16) & 0xff), (uint8_t)((hex >> 8) & 0xff),
                    (uint8_t)(hex & 0xff), (uint8_t)((hex >> 24) & 0xff)};
    }
    return Rgba{(uint8_t)((hex >> 16) & 0xff), (uint8_t)((hex >> 8) & 0xff),
                (uint8_t)(hex & 0xff), 255};
}
Rgba RgbaOpacity(Rgba c, float a01);
// A plain per-channel blend, weighted toward `a`. Not Colorize::mix — that is
// RgbaMixHsl below. This is the arithmetic `default_title_bar_background`
// writes out by hand on the two colours' channels.
Rgba RgbaMix(Rgba a, Rgba b, float t);

// gpui::Hsla — color.rs. Four floats 0..1, which is how GPUI carries a colour
// from the theme all the way to the GPU. This tree carries `Rgba` bytes
// instead, so an Hsla here is the *working* form: the shape a colour is put
// into for the operations Rust does in HSL — lightness, hue, `Colorize::mix`,
// the animation Lerp — and converted straight back out of.
//
// The cost of the difference is quantisation. Rust does a chain of these on
// floats and rounds once at the end; every step here goes back through eight
// bits a channel, so a long chain can drift a byte from what Rust computes.
// That is the same trade the rest of the palette makes (see ToByte in
// gpui.cpp), not a new one.
struct Hsla {
    float h = 0;
    float s = 0;
    float l = 0;
    float a = 0;
};

// gpui::hsla(): the four clamped into 0..1. Rust clamps here and nowhere
// else, so a hue computed past 1 is pinned rather than wrapped.
Hsla HslaNew(float h, float s, float l, float a);
// `impl From<Rgba> for Hsla`. A colour with no lightness left, or all of it,
// reports no saturation — Rust's `l == 0. || l == 1.` arm.
Hsla HslaFromRgba(Rgba c);
// `impl From<Hsla> for Rgba`. Nothing on the way in is clamped; the three
// channels are clamped on the way out, which is what lets a saturation or a
// lightness that ran past 1 land on a colour rather than on nonsense.
Rgba HslaToRgba(Hsla c);
// The two together, for a caller that has four numbers and wants a colour:
// `hsla(h, s, l, a).to_rgb()`.
Rgba RgbaHsla(float h, float s, float l, float a01);
// Colorize::hue: the same color turned to a new hue, keeping its saturation,
// lightness and alpha.
Rgba RgbaWithHue(Rgba c, float h01);
// Colorize::mix: the two colours interpolated in HSL, weighted toward `a`,
// with the hue taking the shorter way round the circle — so red mixed with
// blue is magenta and not the grey the same mix in RGB gives.
Rgba RgbaMixHsl(Rgba a, Rgba b, float factor);

// ─── background ───────────────────────────────────────────────────────────
//
// gpui::Background. A fill is one colour or a two-stop linear gradient, and
// every place that paints a surface takes one — GPUI's `Style::background` is
// a `Fill`, and `.bg(..)` accepts anything that converts into one. A theme
// file spells the gradient the way CSS does:
//
//     "primary.background": "linear-gradient(180deg, #1E293B, #0F172A)"
//
// so the type has to survive from `theme/color.rs`'s parser all the way to
// the D2D / cairo / Core Graphics brush. `color` is the solid fill and, for a
// gradient, its representative colour — the first stop, which is what Rust's
// `try_parse_theme_color` keeps for the flat `ThemeColor` field beside the
// renderable token. Reading `bg.color` off a gradient is therefore never
// wrong, only flat.
struct ColorStop {
    Rgba color = {};
    // Where along the gradient line this stop sits, 0..1.
    float percentage = 0;
};

struct Background {
    Rgba color = {};
    ColorStop from = {};
    ColorStop to = {};
    // CSS degrees: 0 points at the top of the box and turns clockwise, so 90
    // is `to right` and 180 — the default, and what a two-argument
    // `linear-gradient` means — is `to bottom`.
    float angle = 180.f;
    bool gradient = false;

    Background() = default;
    // Implicit, so the several hundred `->Bg(theme.foo)` calls that mean one
    // colour go on saying so. Rust gets the same from `impl From<Hsla> for
    // Background`.
    Background(Rgba c) : color(c) {}
};

// gpui::linear_gradient(angle, from, to).
Background BackgroundLinear(float angle, ColorStop from, ColorStop to);
inline ColorStop ColorStopAt(Rgba c, float pct) {
    return ColorStop{c, pct};
}
// Background::opacity: every stop scaled by the same factor, which is what
// fading a whole element does to its fill.
Background BackgroundOpacity(Background b, float factor);
// Each stop's alpha capped independently at `max` — theme/color.rs's
// `try_parse_background_clamped`. Unlike scaling, a bright `to` stop cannot
// push the rendered highlight past the cap.
Background BackgroundClampAlpha(Background b, float max);
inline bool BackgroundIsSolid(const Background& b) {
    return !b.gradient;
}

// The text-field engine, in the input section below. El and HitRect name one
// before it is defined, the way they name SliderState.
struct InputState;

constexpr float kAuto = -1.f;
constexpr float kFill = -2.f;
constexpr float kPi = 3.14159265358979f;

// ─── theme (Default Dark) ─────────────────────────────────────────────────

struct App;

// ThemeTokens — crates/ui/src/theme/theme_color.rs.
//
// Upstream keeps the palette twice: `ThemeColor`, where every token is one
// `Hsla`, and `ThemeTokens`, where every token is a `ThemeToken { color,
// background }` — the same colour plus the fill it actually paints with,
// which a theme file may spell as a gradient. `Theme` here is the first; this
// is the second, for the tokens `schema.rs` reads with
// `apply_background_color!` rather than `apply_color!`.
//
// A token's `color` always equals the flat field of the same name, so code
// that wants one colour goes on reading `theme.primary` and code that paints
// a surface reads `theme.tokens.primary`. Only the second can be a gradient.
struct ThemeTokens {
    Background background = {};
    Background titleBar = {};
    Background statusBar = {};
    Background tabBar = {};
    Background tabActiveBg = {};
    Background primary = {};
    Background secondary = {};
    Background accent = {};
    Background muted = {};
    Background popover = {};
    Background danger = {};
    Background info = {};
    Background success = {};
    Background warning = {};
    Background progress = {};
    Background scrollbarThumb = {};
    Background scrollbarThumbHover = {};
    Background skeleton = {};
    Background selection = {};
    Background listActive = {};
    Background tableBg = {};
    Background tableActive = {};
    Background tableEven = {};
    Background tableHead = {};
    Background tableFoot = {};
    Background sidebarAccent = {};
    Background sidebarPrimary = {};
    Background overlay = {};
    Background switchThumb = {};
    Background sliderThumb = {};
    // The rest of `apply_background_color!`, in schema.rs's own order. They
    // came in with the theme viewer, which lists every field of Rust's
    // `ThemeColor` and needs a value for each.
    Background button = {};
    Background buttonHover = {};
    Background buttonActive = {};
    Background primaryHover = {};
    Background primaryActive = {};
    Background buttonPrimary = {};
    Background buttonPrimaryHover = {};
    Background buttonPrimaryActive = {};
    Background secondaryHover = {};
    Background secondaryActive = {};
    Background buttonSecondary = {};
    Background buttonSecondaryHover = {};
    Background buttonSecondaryActive = {};
    Background successHover = {};
    Background successActive = {};
    Background buttonSuccess = {};
    Background buttonSuccessHover = {};
    Background buttonSuccessActive = {};
    Background infoHover = {};
    Background infoActive = {};
    Background buttonInfo = {};
    Background buttonInfoHover = {};
    Background buttonInfoActive = {};
    Background warningHover = {};
    Background warningActive = {};
    Background buttonWarning = {};
    Background buttonWarningHover = {};
    Background buttonWarningActive = {};
    Background dangerHover = {};
    Background dangerActive = {};
    Background buttonDanger = {};
    Background buttonDangerHover = {};
    Background buttonDangerActive = {};
    Background accordion = {};
    Background dropTarget = {};
    Background list = {};
    Background listEven = {};
    Background listHead = {};
    Background listHover = {};
    Background sliderBar = {};
    Background switchBg = {};
    Background tab = {};
    Background tabBarSegmented = {};
    Background tableHover = {};
    Background tiles = {};
    Background scrollbarBg = {};
    Background sidebar = {};
    Background groupBox = {};
    Background descListLabel = {};
};

struct Theme {
    Rgba background;
    Rgba foreground;
    Rgba border;
    Rgba mutedFg;
    // input.border, and theme.input_background(): the surface an input paints
    // itself on — the window background in light, the input border at 70% in
    // dark, the way Rust mixes it toward transparent.
    Rgba inputBorder;
    Rgba inputBg;
    // ring: the focus ring color. caret: the text cursor.
    Rgba ring;
    Rgba caret;
    // selection.background: what a run of picked-out text is tinted with. It
    // is a colour of its own in default-theme.json, not `accent` faded — a
    // blue, in both themes.
    Rgba selection;
    // drag_border: the accent a drag paints with — a resize handle under the
    // pointer, a drop target's rule. The same blue in both themes.
    Rgba dragBorder;
    Rgba titleBar;
    Rgba titleBarBorder;
    // status_bar.background, which falls back to the title bar — the two are
    // the same surface at opposite ends of the window, and most themes only
    // name one of them.
    Rgba statusBar;
    // status_bar.border, which falls back to the title bar's the way the
    // surface above falls back to the title bar's own.
    Rgba statusBarBorder;
    Rgba tabBar;
    Rgba tabActiveBg;
    Rgba tabActiveFg;
    Rgba tabFg;
    Rgba tableBg;
    Rgba tableHead;
    Rgba tableHeadFg;
    // table.foot.background / table.foot.foreground, which fall back to the
    // list's head surface and to muted_foreground (theme/schema.rs).
    Rgba tableFoot;
    Rgba tableFootFg;
    Rgba tableRowBorder;
    Rgba tableEven;
    // list.active.background / list.active.border, and the table pair that
    // falls back to them (theme/schema.rs). What ListSettings::active_highlight
    // picks instead of plain `accent` for a selected row: a translucent tint
    // with a solid rule around it, rather than a filled block.
    Rgba listActive;
    Rgba listActiveBorder;
    Rgba tableActive;
    Rgba tableActiveBorder;
    Rgba progress;
    Rgba red;
    Rgba green;
    Rgba blue;
    Rgba yellow;
    Rgba cyan;
    Rgba magenta;
    // base.<hue>.light. A theme file may name them; where it does not they
    // are the background blended with 80% of the base hue, which is what
    // `apply_color!(red_light, fallback = ..)` in theme/schema.rs says. The
    // colour picker's featured row is these twelve.
    Rgba redLight;
    Rgba greenLight;
    Rgba blueLight;
    Rgba yellowLight;
    Rgba cyanLight;
    Rgba magentaLight;
    // chart_1..chart_5, and the pair a candlestick closes on. Both themes
    // give them the same five blues (default-theme.json).
    Rgba chart1;
    Rgba chart2;
    Rgba chart3;
    Rgba chart4;
    Rgba chart5;
    Rgba chartBullish;
    Rgba chartBearish;
    Rgba danger;
    Rgba dangerFg;
    // popover.background / popover.foreground: the surface something floating
    // over the page sits on — a menu, a dropdown, a notification, the find
    // bar. Both default themes give it the window's own background and
    // foreground, so it only parts from them in a theme that says so.
    Rgba popover;
    Rgba popoverFg;
    Rgba secondaryHover;
    Rgba secondaryActive;
    Rgba secondaryFg;
    Rgba secondary;
    Rgba muted;
    Rgba accent;
    // accent.foreground, and the primary pair a theme file can name beside
    // its background (theme/schema.rs). Both are resolved with the same
    // fallbacks Rust gives them: a hover is the background blended with the
    // colour, an active one is the colour darkened.
    Rgba accentFg;
    Rgba primary;
    Rgba primaryFg;
    Rgba primaryHover;
    Rgba primaryActive;
    Rgba sidebar;
    Rgba sidebarFg;
    Rgba sidebarPrimary;
    Rgba sidebarPrimaryFg;
    Rgba sidebarAccent;
    Rgba sidebarAccentFg;
    Rgba sidebarBorder;
    Rgba scrollbarThumb;
    // scrollbar.thumb.hover.background: what the thumb takes while the
    // pointer is on it or a drag has hold of it. Falls back to the thumb's
    // own colour, as schema.rs does.
    Rgba scrollbarThumbHover;
    // scrollbar.background: the track behind the thumb. Transparent in both
    // default themes, which is why nothing but a theme that names it shows
    // one.
    Rgba scrollbarBg;
    Rgba info;
    Rgba infoFg;
    Rgba success;
    Rgba successFg;
    Rgba warning;
    Rgba warningFg;
    Rgba skeleton;
    // switch.thumb.background and slider.thumb.background, both of which fall
    // back to the window background. They are fields of their own only so a
    // theme that spells either as a gradient gets one.
    Rgba switchThumb;
    Rgba sliderThumb;
    // theme.overlay: what a dialog backdrop tints the page with. 5% black in
    // light, 20% in dark (default-theme.json).
    Rgba overlay;
    // group_box.background / group_box.foreground: the surface a filled
    // GroupBox puts its content on.
    Rgba groupBox;
    Rgba groupBoxFg;
    // description_list_label: the label cell of a DescriptionList.
    Rgba descListLabel;
    Rgba descListLabelFg;
    // The rest of `ThemeColor`, which the theme viewer lists and a theme file
    // can name. What paints with them is still, in most cases, the expression
    // the component was written with — the token layer came first and the
    // components follow it one at a time — so a file that names one of these
    // changes the viewer and whatever has been moved over, and no more. Said
    // where it matters in port-progress.md.
    //
    // button.*: the four families a Button has, each with its own foreground,
    // hover and active. The plain one is the input border mixed toward
    // transparent in dark and the window background in light.
    Rgba button;
    Rgba buttonFg;
    Rgba buttonHover;
    Rgba buttonActive;
    Rgba buttonPrimary;
    Rgba buttonPrimaryFg;
    Rgba buttonPrimaryHover;
    Rgba buttonPrimaryActive;
    Rgba buttonSecondary;
    Rgba buttonSecondaryFg;
    Rgba buttonSecondaryHover;
    Rgba buttonSecondaryActive;
    Rgba buttonDanger;
    Rgba buttonDangerFg;
    Rgba buttonDangerHover;
    Rgba buttonDangerActive;
    Rgba buttonSuccess;
    Rgba buttonSuccessFg;
    Rgba buttonSuccessHover;
    Rgba buttonSuccessActive;
    Rgba buttonInfo;
    Rgba buttonInfoFg;
    Rgba buttonInfoHover;
    Rgba buttonInfoActive;
    Rgba buttonWarning;
    Rgba buttonWarningFg;
    Rgba buttonWarningHover;
    Rgba buttonWarningActive;
    // The hover and active halves of the four semantic surfaces, which the
    // palette had only as a background and a foreground.
    Rgba dangerHover;
    Rgba dangerActive;
    Rgba successHover;
    Rgba successActive;
    Rgba infoHover;
    Rgba infoActive;
    Rgba warningHover;
    Rgba warningActive;
    // accordion.background: the surface an accordion item paints, which falls
    // back to the window's own.
    Rgba accordion;
    // drop_target.background: what a dock or a tile paints under a drag that
    // would land there.
    Rgba dropTarget;
    // link / link.active / link.hover: the colour a piece of text that is a
    // link takes. `TextView` draws a markdown link in `link`, which is what
    // node.rs does.
    Rgba link;
    Rgba linkActive;
    Rgba linkHover;
    // list.background and the three rows beside it — the surface the table
    // tokens fall back to.
    Rgba list;
    Rgba listEven;
    Rgba listHead;
    Rgba listHover;
    Rgba tableHover;
    // slider.background: the bar behind a slider's thumb.
    Rgba sliderBar;
    // switch.background: an unchecked switch's track.
    Rgba switchBg;
    // tab.background and tab_bar.segmented.background: a tab that is not the
    // selected one, and the strip a segmented bar paints itself on.
    Rgba tab;
    Rgba tabBarSegmented;
    // tiles.background: the canvas `Tiles` lays its panels on.
    Rgba tiles;
    // window.border: the rule around a window that draws its own frame.
    Rgba windowBorder;
    // crates/ui/src/theme/mod.rs: radius 6, radius_lg 8 (Dialog, Notification).
    float radius;
    float radiusLg;
    // `Theme::radius_full`: the radius that rounds a shape as far as its own
    // size allows — a circle if it is square, a pill if it is not — and zero
    // when the theme squares its corners. Anything past half the shorter side
    // is clamped when it is painted, so this is simply "as round as it goes".
    // Every avatar, badge dot, radio, slider thumb and progress bar takes it
    // rather than half its own height, so one setting governs the lot.
    float radiusFull;
    // The renderable half of the palette, for the tokens a theme file may
    // spell as a gradient. Every one of these carries the flat colour of the
    // same name, so reading a token instead of a field is never wrong.
    ThemeTokens tokens = {};
};

// Every token set to the flat colour of the same name — for a palette built
// in code, and for the tokens a resolved theme file left alone. A token that
// already carries a gradient is kept.
void ThemeTokensReset(Theme* t);

// The tokens schema.rs derives rather than reads: every one whose fallback is
// an expression over the tokens above it. A palette written in code fills
// them by calling this once it has set the rest; `ThemeConfigResolve` calls
// it too, and then lets the file's own words stand over the top. `dark` is
// the mode, which two of the fallbacks read (`active_darken`, and the plain
// button's surface).
void ThemeFillDerived(Theme* t, bool dark);

// ─── Colorize (crates/ui/src/theme/color.rs) ─────────────────────────────
//
// The colour maths every theme fallback is written in. They are here rather
// than beside the registry because the palettes in code derive their own
// tokens with them.

// gpui::transparent_black(), which every `mix_oklab` toward nothing takes.
Rgba RgbaTransparent();
// Hsla::blend: `over` composited onto `base` by its own alpha. The result
// keeps the base's alpha, which is why `background.blend(x)` is opaque
// however faint `x` is.
Rgba RgbaBlend(Rgba base, Rgba over);
// Colorize::lighten / ::darken, which scale the HSL lightness rather than
// mixing toward white or black.
Rgba RgbaLighten(Rgba c, float amount);
Rgba RgbaDarken(Rgba c, float amount);
// Colorize::to_hex: `#RRGGBB`, and `#RRGGBBAA` when the colour is
// translucent. Upstream holds every colour as an `Hsla` and turns it back into
// bytes to print it, truncating each channel — so a colour that arrived as a
// hex string prints one below itself wherever the conversion does not land on
// a byte boundary, and that is the string a reader sees beside a swatch. A
// byte here has not been through that conversion, so the round trip is made
// here before the digits are written. A colour this tree mixed out of an HSL
// of its own is already on the far side of it and should be printed as it
// stands rather than through this.
Str RgbaToHex(Arena* a, Rgba c, bool upper = true);

// Colorize::mix_oklab, which is CSS `color-mix(in oklab, a factor%, b)`: the
// alpha is interpolated first and the Oklab channels are premultiplied by it,
// so mixing toward transparent fades without dragging the hue to black.
Rgba RgbaMixOklab(Rgba a, Rgba b, float factor);

// The semantic token layer is `base/theme_tokens.h`, and the word and
// line boundaries `base/text_boundary.h`: both are gpui-base modules that
// read a `Theme` or a `Str` and are used from `src/base` and up.

enum class ThemeMode : uint8_t {
    Light,
    Dark
};

// The two palettes in force, which is what everything paints from.
const Theme& ThemeDark();
const Theme& ThemeLight();
// ThemeColor::dark() / ::light(): the palette a theme file is resolved
// against, before any config has been applied to it. It never changes, which
// is what keeps two themes applied in a row from compounding.
const Theme& ThemeDefaultDark();
const Theme& ThemeDefaultLight();
// The last step of Theme::apply_config: the resolved palette becomes the
// light or the dark theme. `ui/theme.h` is what produces one.
void ThemeInstall(ThemeMode mode, const Theme& t);
// Theme::font_size and Theme::radius, which the story's Appearance menu
// writes the way Rust writes `Theme::global_mut(cx).font_size`. The themes
// here are shared statics rather than a per-app Global, so a change is the
// process', not one window's. `radius_lg` follows Rust's rule: two more than
// the radius, or nothing when the radius is nothing.
void ThemeSetRadius(float radius);
// The root font size every element inherits from, and what an explicit size
// is measured against: a `Font(12)` is twelve at the default 16, and grows
// with it, which is what Rust gets for free by spelling its sizes in rems.
float ThemeFontSize();
// One wheel notch, in DIPs. GPUI carries a notch as `ScrollDelta::Lines` —
// SPI_GETWHEELSCROLLLINES lines, three by default — and turns it into pixels
// with the line height of the text being scrolled, at the point the delta is
// applied. There is no per-element text style where a wheel event is built,
// so this is the window's own: the theme font size at `gpui::phi()`, which is
// what TextStyle::line_height defaults to. Three lines of 16px text is 78
// DIPs, and a fixed 48 was what this tree scrolled before.
float WheelNotchPixels();
void ThemeSetFontSize(float px);
// The theme belongs to App, the way Rust keeps it as a Global; read it with
// cx->theme(). ThemeNow() is the paint-time fallback for code below Ctx.
// Theme::focus_ring. The ring is painted outside the element's border, so an
// ancestor that clips its content cuts it off; an application whose layout
// clips heavily turns it off here and keeps the tinted border, which takes no
// room. Like the scrollbar mode and the font size, it is one process-wide
// setting rather than one window's Global.
bool ThemeFocusRing();
void ThemeSetFocusRing(bool on);
const Theme& ThemeNow();
void ThemeSet(App* app, ThemeMode mode);
ThemeMode ThemeGet();

// ─── geometry ───────────────────────────────────────────────────────────
//
// crates/gpui/src/geometry.rs. Rust spells these `Point<T>`, `Size<T>`,
// `Bounds<T>` and `Edges<T>`, where `T` is not an element type but a *unit* —
// `Pixels`, `ScaledPixels`, `DevicePixels`, `Rems`, `Length` — so the compiler
// refuses to add device pixels to logical ones. Everything above Paint.h here
// is DIPs and always has been, which leaves that generic with one instantiation
// (`Point<Pixels>` is 170 of the 185 `Point<T>` in gpui-component), so these
// are plain float structs and the arithmetic is written out once. The units
// that are not DIPs get their own named struct instead of a parameter:
// `WinSize` carries both the DIP and the device-pixel size of a window, and the
// backends scale on the way to Direct2D / cairo / Core Graphics.
//
// They are values: aggregates, no constructors, copied by the byte. Code that
// reads or writes one component at a time — the layout pass over `El`, a mouse
// event's position — keeps its flat fields; these are for what is produced,
// returned or passed as a unit.

// gpui::Axis, from the same file. `Along` is the field it picks out.
enum class Axis : uint8_t {
    Horizontal,
    Vertical
};

// The three float shapes are `base::PointF` / `SizeF` / `RectF`, shared with
// the taffy port — one definition, no conversion at the seam. gpui keeps its
// own names for them: `Size` and `Edges` read better in a widget than `SizeF`
// and `RectF` do, and `Edges<Pixels>` is what Rust calls the second one.
//
// `Size` keeps the `.w` / `.h` it always had. `Edges` does not keep its field
// *order*: the shared one is left, right, top, bottom, where Rust's
// Edges<Pixels> is top, right, bottom, left, so a braced `Edges{...}` means
// something different than it used to. `Edges::New(l, r, t, b)` says which.
using Point = base::PointF;
using Size = base::SizeF;
using Edges = base::RectF;

// Bounds<Pixels>. Rust composes it from an origin and a size; here the four
// floats are the struct, so there is no `.origin` or `.size` to reach for.
struct Bounds {
    float x = 0, y = 0, w = 0, h = 0;

    float Right() const { return x + w; }
    float Bottom() const { return y + h; }
    float CenterX() const { return x + w * 0.5f; }
    float CenterY() const { return y + h * 0.5f; }
    // Bounds::contains: the top and left edges are inside, the bottom and
    // right ones are not, so abutting boxes never both claim a point.
    bool Contains(Point p) const {
        return p.x >= x && p.x < x + w && p.y >= y && p.y < y + h;
    }
    // Bounds::inset, which is Bounds::dilate with the amount negated, so a
    // positive amount shrinks the box.
    Bounds Inset(float d) const { return {x + d, y + d, w - d - d, h - d - d}; }
    // Bounds::extend, negated the same way: the content box inside padding.
    Bounds Inset(Edges e) const {
        return {x + e.left, y + e.top, w - e.HorizontalAxisSum(),
                h - e.VerticalAxisSum()};
    }
};

// Bounds::new(origin, size), for the callers that hold the two apart.
inline Bounds BoundsAt(Point origin, Size size) {
    return {origin.x, origin.y, size.w, size.h};
}

// Where a CSS `linear-gradient` puts its two ends inside a box. The gradient
// line runs through the centre at `angle`, and is long enough that the two
// corners it points between land exactly on 0% and 100% — which is what makes
// a 45-degree gradient reach the corners rather than stopping short of them.
// The points come back at the stops' own percentages, so the caller hands the
// backend two colours and two positions and nothing else: all three clamp
// beyond their ends (D2D's default extend, cairo's PAD, Core Graphics' draws-
// before/after), so a stop at 25% still paints the quarter behind it.
void BackgroundLine(const Background& b, Bounds box, Point* p0, Point* p1);

// ─── entities ─────────────────────────────────────────────────────────────
//
// GPUI keeps view state in `App` and hands out `Entity<T>` handles; a view
// implements `Render` and mutates itself through `Context<T>`. The same shape
// here, minus the refcounting: `App` owns the state, `Entity<T>` is a POD
// generational handle, and `Ctx` is the one context type (GPUI splits it into
// `&mut App` / `&mut Window` / `&mut Context<T>` only to satisfy the borrow
// checker).

struct Window;
struct Ctx;
struct El;
struct SliderState;
// The shaped run a text element measured to; Paint.h owns the type.
struct TextLayout;

struct EntityId {
    int32_t index = -1;
    uint32_t gen = 0; // 0 == null handle

    bool IsValid() const { return index >= 0 && gen != 0; }
};

inline bool operator==(EntityId a, EntityId b) {
    return a.index == b.index && a.gen == b.gen;
}
inline bool operator!=(EntityId a, EntityId b) {
    return !(a == b);
}

using RenderFn = El* (*)(void* self, Ctx* cx);
using DropFn = void (*)(void* self);

struct EntitySlot {
    void* ptr = nullptr;
    uint32_t gen = 0;
    RenderFn render = nullptr;
    DropFn drop = nullptr;
};

// Where one transition has got to, kept per window and per id. Separate from
// KeyedSlot because the lifetime is GPUI's element state rather than Rust's
// keyed state: a slot nothing asked for while the frame was built is dropped,
// so a dialog that closes and opens again animates its way in a second time.
struct MotionSlotRec {
    uint32_t key = 0;
    // The frame that last asked for it. `frameSeq` at the time, so the sweep
    // is a comparison rather than a flag to clear.
    uint64_t frame = 0;
    void* ptr = nullptr;
};

// window.use_keyed_state: per-window state owned by a RenderOnce element that
// has nowhere else to keep it.
struct KeyedSlot {
    uint32_t key = 0;
    void* ptr = nullptr;
    DropFn drop = nullptr;
    // Set when the slot was taken through KeyedEntity: the app owns the
    // memory then, and the window only remembers which entity the key means.
    EntityId entity = {};
};

// ─── mouse input ──────────────────────────────────────────────────────────
// crates/gpui/src/interactive.rs, field for field, minus the four things a
// C++ struct cannot say the same way:
//   * `MouseButton::Navigate(NavigationDirection)` carries its direction; a
//     C++ enumerator cannot, so the two directions are their own constants.
//   * `Option<MouseButton>` on a move is a `pressed` flag plus the button.
//   * `ScrollDelta::Pixels | Lines` is a delta plus `precise`. Rust defers the
//     multiply to `pixel_delta(line_height)`; the three windows here turn a
//     notch into DIPs at the seam, so nothing downstream needs a line height.
//   * `Point<Pixels>` is `x` and `y`. There is a `Point` above, but an
//     event's position is read a component at a time, and flattening it
//     spares every handler a `.position`.
// What is missing outright: touch, pinch and pressure, which none of these
// three windows report.

enum class MouseButton : uint8_t {
    Left,
    Right,
    Middle,
    NavigateBack,
    NavigateForward
};

// GPUI's Modifiers. `platform` is the Windows key, X11's Super and macOS's
// Command; `function` is Fn, which only macOS reports.
struct Modifiers {
    bool control = false;
    bool alt = false;
    bool shift = false;
    bool platform = false;
    bool function = false;

    bool Modified() const {
        return control || alt || shift || platform || function;
    }
    // The semantically secondary modifier: Command on macOS, Control on the
    // other two — Modifiers::secondary().
    bool Secondary() const {
#if GPUI_OS_MAC
        return platform;
#else
        return control;
#endif
    }
    int Count() const {
        return (int)control + (int)alt + (int)shift + (int)platform +
               (int)function;
    }
};

// The phase of a scroll gesture. A wheel notch is one Moved; a trackpad on
// macOS runs Started -> Moved -> Ended.
enum class TouchPhase : uint8_t {
    Started,
    Moved,
    Ended,
    Cancelled
};

// DispatchPhase, from GPUI's `Window::dispatch_event`. A mouse event is
// offered to the chain of elements under the pointer twice: outside-in in the
// Capture phase, where an ancestor can pre-empt what is inside it, and then
// inside-out in the Bubble phase, which is where a handler that only cares
// about its own element sits. `WindowStopPropagation` is `cx.stop_propagation`
// — the rest of the chain does not hear it.
enum class DispatchPhase : uint8_t {
    Capture,
    Bubble
};

struct MouseDownEvent {
    MouseButton button = MouseButton::Left;
    float x = 0;
    float y = 0;
    // The box of the element the press landed on, when it has an identity —
    // the same thing ClickEvent::el carries, and what a handler needs to
    // place something where the press was inside it. A Rust hitbox has the
    // bounds too; the event does not, because the closure already has them.
    Bounds el = {};
    Modifiers modifiers = {};
    // How many presses this one is in an unbroken run: 1, 2, 3… What Rust's
    // on_double_click tests — `on_click(|ev, ..| ev.click_count() == 2)`.
    int clickCount = 1;
    // The press that also activated the window. Windows knows from
    // WM_MOUSEACTIVATE; X11 has no such notion and a Cocoa view does not
    // accept the first mouse, so it is false on those two.
    bool firstMouse = false;
    // Which pass of the chain this is. A handler registered for one phase only
    // ever sees that phase; the field is there for one that took both.
    DispatchPhase phase = DispatchPhase::Bubble;

    // MouseDownEvent::is_focusing.
    bool IsFocusing() const { return button == MouseButton::Left; }
};

struct MouseUpEvent {
    MouseButton button = MouseButton::Left;
    float x = 0;
    float y = 0;
    // The box of the element that heard it, the way MouseDownEvent carries
    // one. The chain fills it in as the event walks.
    Bounds el = {};
    Modifiers modifiers = {};
    int clickCount = 1;
    DispatchPhase phase = DispatchPhase::Bubble;

    bool IsFocusing() const { return button == MouseButton::Left; }
};

struct MouseMoveEvent {
    float x = 0;
    float y = 0;
    // Rust's Option<MouseButton>: `pressed` is the Some, `pressedButton` its
    // value. With no button down, pressedButton means nothing.
    bool pressed = false;
    MouseButton pressedButton = MouseButton::Left;
    Modifiers modifiers = {};

    // MouseMoveEvent::dragging.
    bool Dragging() const {
        return pressed && pressedButton == MouseButton::Left;
    }
};

// Two strings with the same bytes. `Str` is a pointer and a length, so a
// kind that names a drag is compared by what it says, not by where it lives.
inline bool StrSame(Str a, Str b) {
    return a.len == b.len &&
           (a.len == 0 || memcmp(a.s, b.s, (size_t)a.len) == 0);
}

// What a drag carries. Rust's `on_drag(payload, ..)` takes a value of any
// type and `DragMoveEvent<T>` only reaches the handlers that named that type;
// there is no type to match on here, so the payload says what kind of thing
// is being dragged by name and which one of that kind by index.
struct DragPayload {
    Str kind = {};
    int ix = 0;
    void* data = nullptr;

    bool IsValid() const { return kind.s != nullptr; }
};

// DragMoveEvent<T>: what is being dragged, and the move that carried it.
struct DragMoveEvent {
    DragPayload drag = {};
    MouseMoveEvent event = {};
    // The dragged element's box, which is `bounds` on the entity the drag
    // names in Rust. It is the box the last frame laid out, so a handler that
    // moves the element reads its own answer back on the next move.
    Bounds el = {};
};

// on_drop::<T>: a drag that let go over this element. Rust matches the drop
// handler by the payload's type; here the element says which `kind` it takes,
// and a drag carrying anything else passes over it as if it were not there.
struct DropEvent {
    DragPayload drag = {};
    // Where the button came up, in window coordinates.
    float x = 0;
    float y = 0;
    // The box of the element that took the drop, so a handler can work out
    // where inside itself the drop landed.
    Bounds el = {};
};

// The pointer left the window. GPUI's MouseExitEvent is a MouseMoveEvent in
// all but name — it derefs to one — so it carries the same three things.
struct MouseExitEvent {
    float x = 0;
    float y = 0;
    bool pressed = false;
    MouseButton pressedButton = MouseButton::Left;
    Modifiers modifiers = {};
};

struct ScrollWheelEvent {
    float x = 0;
    float y = 0;
    // DIPs, already scaled: one wheel notch is 48. Positive scrolls the view
    // up and left, which is the sign WM_MOUSEWHEEL reports.
    float deltaX = 0;
    float deltaY = 0;
    // ScrollDelta::Pixels rather than ::Lines — a trackpad or another device
    // that measures the gesture itself, not a wheel with detents.
    bool precise = false;
    Modifiers modifiers = {};
    TouchPhase phase = TouchPhase::Moved;
};

// GPUI's PlatformInput: what a platform window hands to the window layer.
// Rust's enum carries its payload; here a kind and a union of the same structs
// do. Only the mouse variants exist — keys still arrive through WindowKeyDown
// and WindowChar, whose KeyEvent is a merged key-and-character event rather
// than GPUI's Keystroke, and nothing here produces a file drop or a gesture.
enum class PlatformInputKind : uint8_t {
    MouseDown,
    MouseUp,
    MouseMove,
    MouseExited,
    ScrollWheel
};

struct PlatformInput {
    PlatformInputKind kind = PlatformInputKind::MouseMove;
    union {
        MouseDownEvent mouseDown = {};
        MouseUpEvent mouseUp;
        MouseMoveEvent mouseMove;
        MouseExitEvent mouseExited;
        ScrollWheelEvent scrollWheel;
    };
};

// GPUI's ClickEvent is the down and up pair (ClickEvent::Mouse) or the Enter
// or Space that activated a focused element (ClickEvent::Keyboard). This one
// is flat: it fires from the release, like Rust's, and carries the position
// the release landed at with the count and the modifiers the press had. It
// also carries what our hit rect knows and a Rust hitbox does not have to —
// which element this was, and where it is.
struct ClickEvent {
    float x = 0;
    float y = 0;
    MouseButton button = MouseButton::Left;
    // The element's click id, when it has one. Lets one handler serve a list.
    int id = 0;
    // The box that was hit, so a handler can place the click inside it — what
    // a slider needs to turn a press on its track into a value. This is also
    // KeyboardClickEvent::bounds, the only position a keyboard click has.
    Bounds el = {};
    int clickCount = 1;
    Modifiers modifiers = {};
    // ClickEvent::Keyboard: Space or Enter on the focused element, with no
    // pointer anywhere near it. ClickEvent::is_keyboard.
    bool keyboard = false;
    // KeyboardClickEvent::button: which of the two activated it, KeyReturn or
    // KeySpace. 0 when the pointer made this click.
    int keyboardKey = 0;
};

// Portable key codes. The values are the Win32 VK_* ones, so the Windows
// window passes wParam straight through and the X11 window maps keysyms onto
// them. Only the keys the widgets react to are named.
enum {
    KeyBack = 8,
    KeyTab = 9,
    KeyReturn = 13,
    KeyShift = 16,
    KeyControl = 17,
    KeyMenu = 18,
    KeyEscape = 27,
    KeySpace = 32,
    KeyPageUp = 33,
    KeyPageDown = 34,
    KeyEnd = 35,
    KeyHome = 36,
    KeyLeft = 37,
    KeyUp = 38,
    KeyRight = 39,
    KeyDown = 40,
    KeyDelete = 46,
    // Letters and digits are their ASCII uppercase / digit codes.
    KeyA = 65,
    KeyC = 67,
    KeyE = 69,
    KeyF = 70,
    KeyH = 72,
    KeyV = 86,
    KeyX = 88,
    KeyY = 89,
    KeyZ = 90,
    // The two OEM keys a field binds: VK_OEM_4 and VK_OEM_6, which the X11
    // and Cocoa windows map their bracket keys onto.
    KeyLeftBracket = 219,
    KeyRightBracket = 221
};

struct KeyEvent {
    int vk = 0;      // a Key* code, 0 for a typed character
    uint32_t ch = 0; // typed codepoint, 0 for key down/up
    bool down = false;
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
    // Command on macOS, the Windows/Super key elsewhere.
    bool platform = false;
    // cx.propagate(): an `El::OnKeyDown` handler that leaves this true passes
    // the keystroke on outwards, the way an action handler does. Unused by
    // the window-level `WindowOnKey`, which is the last thing to see a key.
    bool propagate = true;
};

// The pointer shape the window asks the OS for. GPUI spells this
// CursorStyle and has a dozen; these are the two the element tree can tell
// apart today.
enum class CursorKind : uint8_t {
    Arrow,
    IBeam,
    // cursor_pointer: the hand, which says a thing is there to be clicked.
    // GPUI's own default for a div is the arrow, so this is opt-in; Rust asks
    // for it on links and on the button variants that look like one.
    Pointer,
    // cursor_col_resize, which a table's column edge asks for.
    ColResize,
    // cursor_row_resize: the handle between two panels stacked one over the
    // other.
    RowResize,
    // cursor_crosshair: a canvas that is drawn on rather than clicked.
    Crosshair
};

// Fired by a window timer; GPUI does this with cx.spawn + Timer::after.
struct TickEvent {
    int ms = 0;
};

// What GPUI's `on_hover` hands its closure: a `&bool` saying whether the
// pointer just entered the element or just left it. It fires on the change,
// not on every move inside.
struct HoverEvent {
    bool hovered = false;
};

// cx.listener(...): a handler plus the entity it runs against. Dispatch looks
// the entity up and drops the event if the handle went stale.
//
// `arg` is what the Rust closure would have captured — the tab index in
// `cx.listener(move |this, _, _, cx| this.tab = ix)`. Without it a view has to
// hand out element ids and decode them again in one big switch.
using ListenerFn = void (*)(void* self, Ctx* cx, const void* ev);
using ListenerArgFn = void (*)(void* self, Ctx* cx, const void* ev,
                               intptr_t arg);

struct Listener {
    void* fn = nullptr;
    EntityId view = {};
    intptr_t arg = 0;
    // The handler takes an argument at all.
    bool hasArg = false;
    // The caller already said what it is. Rust hands a closure both what it
    // captured and what the widget produced; there is one slot here, so a
    // value the caller bound wins and ListenerFill leaves it alone.
    bool argBound = false;

    bool IsValid() const { return fn != nullptr; }
};

// One armed timer. GPUI has no timer list: it spawns a task per timer and the
// Task handle cancels on drop. Here the window keeps them, and dispatch drops
// one whose view went stale — which is the same lifetime, spelled differently.
struct TimerSub {
    int id = 0; // what WindowCancelTimer takes
    int ms = 0;
    double dueAt = 0; // TimeNow() deadline
    bool repeat = false;
    Listener l;
};

// ─── style / element ──────────────────────────────────────────────────────

enum class ElKind : uint8_t {
    Div,
    Text,
    Chart,
    Progress,
    Icon,
    // gpui's img(..): a decoded bitmap, sized by its own pixels unless the
    // caller says otherwise. An image whose source cannot be decoded — a
    // remote URL, a format the platform does not read — paints its `text`
    // instead, which is the alt text a document gave it.
    Image
};

// gpui's Display. `div()` is a block container, the way an unstyled HTML
// element is: children stack down the page at the container's full width, and
// nothing is stretched or shrunk to make them fit. `flex()` — or either of the
// h_flex/v_flex helpers that call it — is what turns on the flex model.
enum class Display : uint8_t {
    Block,
    Flex
};

enum class FlexDir : uint8_t {
    Row,
    Col,
    // flex_row_reverse / flex_col_reverse: the same axis, laid out from the
    // far end. A toolbar that pins its buttons to the right without asking
    // for a justification is written this way.
    RowReverse,
    ColReverse
};
enum class Align : uint8_t {
    Start,
    Center,
    End,
    Stretch
};
enum class Justify : uint8_t {
    Start,
    Center,
    End,
    SpaceBetween,
    SpaceAround
};
// gpui's Overflow, per axis: `overflow_hidden` clips and
// `overflow_x_scroll` / `overflow_y_scroll` scroll.
enum class Overflow : uint8_t {
    Visible,
    Hidden,
    Scroll
};

// ScrollbarMode, crates/base/src/scrollbar.rs. Rust's default is Scrolling —
// the bar is up while the offset moves, holds for FADE_OUT_DELAY idle seconds
// and then fades over the rest of FADE_OUT_DURATION. It needs a clock per
// scroll area, which is keyed off `El::ScrollId` the way Rust keys its state
// off the element id; an area with no id of its own has nowhere to keep the
// clock and stays up. Our theme default is still Always — a story shot of a
// scrollable page should show its bar — and the story's Appearance menu
// offers all three.
enum class ScrollbarMode : uint8_t {
    Always,
    Hover,
    Scrolling
};

// FADE_OUT_DELAY / FADE_OUT_DURATION, in seconds. The curve between them is
// Rust's `1 - (elapsed - delay)^10`: flat for most of the second, then a
// drop off the end.
// `WindowOptions::inactive_frame_interval`: how long a window that is not
// the active one waits between animation frames. 500 ms caps background
// animation at 2 FPS, which is what the story app asks for upstream.
const double kInactiveFrameInterval = 0.5;

// RADIUS_FULL: past half the shorter side, so the paint clamps it to exactly
// as round as the box goes.
const float kRadiusFull = 9999.f;

// The stacking layers a frame paints in, in the order it paints them. Rust
// gives a deferred element a `with_priority` and lets GPUI's scene sort on
// it — `POPUP_PRIORITY` is 100 and `TOOLTIP_PRIORITY` is 200, which is what
// keeps a tip above a dialog or a popup — and the walks here already run in
// that order, so what the numbers have to do is keep the same relation.
const int kPaintLayerTree = 0;
// The deferred and fixed elements the tree painted over: popups, dialogs,
// menus. Rust's POPUP_PRIORITY.
const int kPaintLayerPopup = 1;
// TooltipOverlay, over everything the frame drew.
const int kPaintLayerTooltip = 2;
// The inspector's highlights, which GPUI paints over everything.
const int kPaintLayerInspector = 3;

// crates/base/src/scrollbar.rs `ScrollbarEntrance`: how a bar arrives. The
// styled layer chooses the choreography; base only plays it.
enum class ScrollbarEntrance : uint8_t {
    // Fade in without moving.
    Fade,
    // Slide in from the nearest edge while fading.
    SlideAndFade
};

// `ScrollbarMotion`, in seconds rather than Duration. Base installs no motion
// of its own — every duration there defaults to zero and both visibility and
// thumb width snap — and the timing below is what crates/ui projects onto it
// in `theme/mod.rs`'s `scrollbar_motion`.
struct ScrollbarMotion {
    // How long visibility is held after the last scroll, drag, or hover.
    float idle = 2.f;
    // How long the bar takes to become fully visible.
    float enter = 0.3f;
    // How long it takes to fade away once the idle hold expires.
    float exit = 0.5f;
    // How long the thumb takes to reach a new width.
    float expand = 0.3f;
    ScrollbarEntrance entrance = ScrollbarEntrance::Fade;
    ScrollbarEntrance thumbHoverEntrance = ScrollbarEntrance::Fade;
};

// The motion this design system projects for a mode. Scrolling and track
// hover reveal a bar by fading it in place; in hover mode, pointing at the
// thumb slides it in from the nearest edge as it fades. Reduced motion zeroes
// every duration, which is how a policy with no motion arrives here.
ScrollbarMotion ScrollbarMotionFor(ScrollbarMode mode);

// What `VisibilityAnimation::sample` answers: how much of the bar is there,
// how far along its slide it is, and whether it is still moving.
struct ScrollbarVisibility {
    float opacity = 0;
    float position = 0;
    bool running = false;
};

// The animation itself, keyed by scroll id the way the painted bars are.
// These are the seam tests/ScrollbarTests.cpp drives the curves through —
// `now` is the clock rather than TimeNow(), so a test can step it — and
// nothing else needs them: a frame reaches the same state through paint.
void ScrollbarVisibilitySet(int scrollId, bool visible,
                            ScrollbarEntrance entrance, float enter, float exit,
                            double now);
ScrollbarVisibility ScrollbarVisibilityAt(int scrollId, double now);
// `visibility_translation`: how far off its edge the bar is drawn at that
// point in the slide. Zero once it has arrived.
float ScrollbarSlideOffset(float trackWidth, float position);

// The default Theme::scrollbar_mode. An element that names its own wins, the
// way `Scrollbar::new().mode(..)` overrides the theme's.
ScrollbarMode ScrollbarModeNow();
void ScrollbarModeSet(ScrollbarMode m);
// Drops what the Scrolling bars remember. The app's own teardown; a caller
// has no reason to.
void ScrollFadeClear();

enum class IconName : uint8_t {
    None = 0,
    Inbox,
    Bot,
    Cpu,
    MemoryStick,
    HardDrive,
    Battery,
    BatteryCharging,
    BatteryMedium,
    BatteryFull,
    WindowMinimize,
    WindowMaximize,
    WindowRestore,
    WindowClose,
    LayoutDashboard,
    Calendar,
    Folder,
    Settings,
    GalleryVerticalEnd,
    CircleUser,
    User,
    PanelLeft,
    PanelLeftOpen,
    PanelLeftClose,
    PanelRight,
    PanelRightOpen,
    PanelRightClose,
    PanelBottom,
    PanelBottomOpen,
    Info,
    X,
    CircleCheck,
    TriangleAlert,
    CircleX,
    Loader,
    LoaderCircle,
    Ellipsis,
    ChevronsUpDown,
    SquareTerminal,
    BookOpen,
    Settings2,
    Frame,
    ChartPie,
    File,
    FolderOpen,
    ChevronDown,
    ChevronLeft,
    ChevronRight,
    // The find bar's two: the case toggle and the replace-mode toggle.
    CaseSensitive,
    Replace,
    ChevronUp,
    Check,
    Search,
    Minus,
    Plus,
    Palette,
    Copy,
    Bell,
    Star,
    StarFill,
    Eye,
    EyeOff,
    Heart,
    ArrowLeft,
    ArrowRight,
    ArrowUp,
    ArrowDown,
    Building2,
    Asterisk,
    Sun,
    Moon,
    Play,
    Maximize,
    Minimize,
    Map,
    Globe,
    Github,
    ExternalLink,
    HeartOff,
};

struct PaintCtx;

// A run of a text element painted differently from the rest of it: GPUI's
// HighlightStyle over a range, which is what a syntax highlighter's captures
// and an editor's TextDecorations both come to. The runs are UTF-8 offsets
// into the element's own text and must not overlap; whatever they leave over
// paints in the element's own colour.
//
// Weight and slant are not here. A run drawn in another face would shape to
// other widths, and every run of an element shares one shaped layout — the
// colour, the wash behind it and the rule under it are what can change
// without re-shaping.
struct TextSpan {
    int lo = 0;
    int hi = 0;
    Rgba color = {};
    // The wash behind the run, painted before the glyphs. Alpha 0 is none,
    // which is what a run that only recolours its glyphs wants — and the
    // reason it is spelled out: an Rgba defaults to opaque.
    Rgba bg = {0, 0, 0, 0};
    // UnderlineStyle: a rule under the run in its own colour.
    bool underline = false;
    // UnderlineStyle::wavy — the squiggle a diagnostic is marked with.
    bool wavy = false;
};

// Which of crates/ui/src/chart's charts this series is. They share the axis,
// the grid and the labels; what differs is the shape drawn over them.
enum class ChartKind : uint8_t {
    Area,
    Line,
    Bar,
    Candlestick,
    Radar
};

// plot::StrokeStyle. How a run of points is joined: the Catmull-Rom curve
// GPUI draws by default, straight segments, or a stair that steps after each
// point.
enum class ChartStroke : uint8_t {
    Natural,
    Linear,
    StepAfter
};

// BarAlignment: which edge of the plot a bar grows from. Bottom is the usual
// column; Left and Right lay the bands down the side and make it a row chart.
enum class BarAlign : uint8_t {
    Bottom,
    Top,
    Left,
    Right
};

// One more band or line over the same axes. Rust's AreaChart takes a `y`
// accessor per series — `.y(..).stroke(..).fill(..).name(..)`, as many times
// as there are series — and they share one domain and one grid. This is that
// list, past the first, which lives on the ChartSeries itself.
struct ChartSeriesExtra {
    const float* ys = nullptr;
    Rgba stroke = {};
    Rgba fillTop = {};
    Rgba fillBot = {};
    // The series' own name in the tooltip, the way `name(..)` sets it.
    Str name = {};
};

struct ChartSeries {
    ChartKind kind = ChartKind::Area;
    const float* ys = nullptr;
    int n = 0;
    // The series after the first. They share `n`, the domain and the axes.
    const ChartSeriesExtra* more = nullptr;
    int nMore = 0;
    int tickMargin = 15;
    // The x-axis labels, one per point; without them the index is drawn.
    const char* const* labels = nullptr;
    // A second series drawn over the first, as a stacked area chart does.
    bool overlay = false;
    Rgba stroke = {};
    Rgba fillTop = {};
    Rgba fillBot = {};
    // The value domain the y axis is scaled to. Both zero takes it from the
    // data, which is what a ScaleLinear over the data's own extent does; the
    // system monitor's charts say 0..100 instead.
    float domainMin = 0;
    float domainMax = 0;
    // Candlestick: the other three values per point, and the two colors a
    // candle takes depending on which way it closed.
    const float* opens = nullptr;
    const float* highs = nullptr;
    const float* lows = nullptr;
    Rgba up = {};
    Rgba down = {};
    // Bar: ScaleBand's inner padding, and how round the top of a bar is.
    float bandPadding = 0.2f;
    float barRadius = 4;
    BarAlign barAlign = BarAlign::Bottom;
    // Stack: where each bar starts, so a series drawn over another one sits
    // on top of it rather than in front of it. Null starts every bar at zero.
    const float* bases = nullptr;
    // BarChart::label: the value written at the bar's growing end.
    bool barLabels = false;
    // BarChart::value_axis: tick labels down the value axis, which reserve
    // kValueAxisGap along the band axis for themselves.
    bool valueAxis = false;
    // BarChart::value_tick_count: how many even intervals the value axis is
    // divided into, which drives the grid spacing and the labels alike. A
    // count, unlike tickMargin, which is a stride over the band categories.
    int valueTickCount = 4;
    // BarChart::fill(|d, ..|): a colour per bar rather than one for the lot.
    const Rgba* barFills = nullptr;
    // BarChart::fill_gradient: the two stops a bar is filled between. Run
    // across the chart's own range by default, or down each bar on its own
    // when barGradientPerBar is set.
    bool barGradient = false;
    bool barGradientPerBar = false;
    // fill(|_, bar, chart, _|): one ramp across the whole plot, on its
    // bottom-left to top-right diagonal, with every bar filled by the slice
    // of it that falls under its own footprint. The stops then differ from
    // bar to bar, which the other two modes' single pair cannot express.
    bool barGradientDiagonal = false;
    Rgba barFillFrom = {};
    Rgba barFillTo = {};
    // StrokeStyle and LineChart::dot, both of which the area chart shares.
    ChartStroke strokeStyle = ChartStroke::Natural;
    bool dot = false;
    // CandlestickChart::body_width_ratio: how much of a band the body takes.
    float bodyWidthRatio = 0.8f;
    // RadarChart::outer_radius / grid_levels, and its own dot flag.
    float radarRadius = 0;
    int gridLevels = 4;
    // AreaChart::id in Rust: a chart with one takes the pointer, and shows a
    // crosshair and a tooltip for whatever it is over.
    bool tooltip = false;
    // The name the tooltip's row goes by.
    Str name = {};
};

// Corners<Pixels>: a radius per corner, which is what `rounded_tl(..)` and its
// three siblings build. `Style::radius` is the four of them at once and stays
// what almost everything says; this is for a box whose corners differ, which
// is a control butted up against its neighbour — a NumberInput's step buttons
// round only the outer pair, to follow the frame around them.
struct Corners {
    float tl = 0;
    float tr = 0;
    float br = 0;
    float bl = 0;

    bool IsUniform() const { return tl == tr && tr == br && br == bl; }
};

struct Style {
    Display display = Display::Block;
    FlexDir dir = FlexDir::Row;
    Align align = Align::Stretch;
    Justify justify = Justify::Start;
    Overflow overflowY = Overflow::Visible;
    Overflow overflowX = Overflow::Visible;
    float width = kAuto;
    float height = kAuto;
    // w_1_2 / w_2_3 / …: a fraction of the parent's content box, which GPUI
    // has as first-class widths. 0 = unset.
    float widthFrac = 0;
    // min_width / min_height. kAuto is CSS's `auto`, the content-based
    // automatic minimum size, and is what an element that never names one
    // gets. An explicit zero is a different thing — Rust's `min_w_0()`, the
    // idiom for "this may shrink past its content" — so the two cannot share
    // a sentinel.
    float minW = kAuto;
    float minH = kAuto;
    float maxW = 1e9f;
    float maxH = 1e9f;
    // aspect_ratio, width over height. Only an image sets it, and it sets it
    // from the decoded bitmap: gpui's `Img::request_layout` stamps the ratio
    // on the style so a clamped width carries the height with it. 0 = unset.
    float aspect = 0;
    float flexGrow = 0;
    float flexShrink = 1;
    // flex-basis, the main size a flex item starts from before grow and
    // shrink are handed the leftover. kAuto is CSS's `auto` — start from the
    // item's own width or height — and is what every element that never says
    // otherwise gets. Zero is what `flex_1()` means, and it is a different
    // instruction: siblings then split the line by their grow factors alone,
    // rather than each keeping its content's width and splitting only what
    // is left over.
    float flexBasis = kAuto;
    // flex-basis as a fraction of the line, which is `relative(f)` in Rust.
    // Zero is unset, and a basis in DIPs is the field above.
    float flexBasisFrac = 0;
    Edges pad = {};
    // gap, per axis. `Gap()` sets both, which is what `gap_N` does; a style
    // that names only one — `gap_x_2` — leaves the other where it was.
    float gapX = 0;
    float gapY = 0;
    float border = 0;
    float borderT = 0;
    float borderB = 0;
    float borderL = 0;
    float borderR = 0;
    float radius = 0;
    // The four corners, when they are not all `radius`. `hasCorners` is what
    // says to read them at all, so the ordinary case costs one bool.
    Corners corners = {};
    bool hasCorners = false;
    Background bg = {};
    Rgba borderColor = {};
    Rgba color = {};
    // Transformation::rotate: turns clockwise about the element's own centre,
    // where 1 is a whole one. Only an icon reads it — a rotated box would want
    // the whole element tree in on it, and nothing in the port asks for one.
    float rotate = 0;
    // Style::opacity. 1 is untouched; anything less fades this element and
    // everything under it.
    float opacity = 1;
    float fontSize = 0; // 0 = inherit
    // line_height as a multiple of the font size. 0 = GPUI's default, phi.
    float lineHeight = 0;
    bool truncate = false;
    bool wrap = false;
    // flex_wrap on a row: children that do not fit start a new line.
    bool flexWrap = false;
    bool hasBg = false;
    bool hasColor = false;
    bool fontBold = false;
    bool fontSemibold = false;
    bool fontMedium = false; // font_medium(): DWrite weight 500
    bool fontMono = false;   // font_family("Consolas")
    bool underline = false;  // text_decoration_1()
    // text_decoration_line_through(): a ~~del~~ run, an HTML <s> or <del>.
    bool strike = false;
    bool italic = false; // *emphasis*
    bool borderDashed = false;
    // Dash on/off lengths for a dashed border, in stroke widths. GPUI's
    // border_dashed draws 2 on, 1 off; a dashed Separator paints its own path
    // with 4 on, 2 off.
    float dashOn = 2;
    float dashOff = 1;
    bool absolute = false;
    bool fixed = false; // out-of-flow in window coords (Rust deferred overlay)
    // Laid out where it sits, painted after everything else — GPUI's
    // deferred(): a popup anchored to its trigger still draws over the page
    // below it, and hit-tests before it.
    bool deferred = false;
    // Side placement rather than corner anchoring: the requested side when the
    // popup fits there, the opposite side when it does not, and the roomier of
    // the two when neither does — `Positioner::side`, which is what a dropdown
    // uses upstream so a menu near the bottom of the window opens upward
    // instead of being clamped against the edge.
    bool anchorFlip = false;
    bool anchorBelow = false;   // absolute, just under the parent box
    bool anchorAbove = false;   // absolute, just over it
    bool anchorCenterX = false; // absolute, centered on the parent box
    float anchorGap = 0;
    float absTop = kAuto, absLeft = kAuto, absBottom = kAuto, absRight = kAuto;
    // left(relative(f)) / right(relative(f)): the offset is that fraction of
    // the parent's width, added to the pixel one. A stepper's connector needs
    // it to reach from the middle of one step to the middle of the next.
    float absLeftRel = 0, absRightRel = 0;
    Background hoverBg = {};
    bool hasHoverBg = false;
    // hover(|style| style.text_color(..)): what the subtree under a hovered
    // element paints with, for the descendants that set no color of their own.
    Rgba hoverFg = {};
    bool hasHoverFg = false;
    // active(|style| style.bg(..)): what the box paints with while it is held
    // down. GPUI's `clicked_state` is set by the press and cleared by the
    // release, so it stays on while the pointer slides off the element —
    // which is what lets a reader see a button still pressed as they move
    // away from it, and see it come back to hover when they move back.
    Background activeBg = {};
    bool hasActiveBg = false;
    // div().group(""): this element is the group a descendant's
    // `group_hover` is resolved against, and the pointer being anywhere in
    // its box is what counts as hovered — not the pointer being on this
    // element rather than on something drawn over it.
    bool group = false;
    // .invisible().group_hover("", |this| this.visible()): the element keeps
    // its box in layout and is simply not drawn while the group around it is
    // not hovered, which is how a card's close button takes its corner
    // whether or not it is showing.
    bool groupHoverVisible = false;
    // .group_hover("", |this| this.bg(c)): the fill while the pointer is
    // anywhere inside the group, which is a different question from this
    // element being the hovered one. The element's own hover and active
    // fills still win over it.
    Background groupHoverBg = {};
    bool hasGroupHoverBg = false;
    int focusId = 0;
    // El::PathId asked for the focus id to be the element's path, which is
    // only known once the tree is built. An explicit FocusId(v) — including
    // FocusId(0), which is how a decorated wrapper stays out of the tab
    // order — clears this and wins.
    bool focusFromPath = false;
    // FocusHandle::tab_index / tab_stop. The index groups the traversal: Tab
    // visits every element of the lowest index in the order they were painted,
    // then the next index, and so on, which is how a control can be reached
    // before one that is laid out above it. A handle that is not a tab stop
    // keeps its focus and its ring and is simply skipped by the traversal —
    // an input's clear button, or a dock tab bar's tools, which the keyboard
    // reaches through the thing they belong to rather than one at a time.
    int tabIndex = 0;
    bool tabStop = true;
    // Whether a press on this element moves focus to it. GPUI's `track_focus`
    // does not: every widget in gpui-component that takes focus from a click
    // calls `focus_handle.focus(window, cx)` itself — the input, the otp
    // field, the tree, the list, the select and the colour picker do, and the
    // button, the checkbox, the radio, the switch and the link do not. A
    // press used to focus anything with a handle here, which is why a clicked
    // button kept a focus ring the Rust one never shows.
    bool focusOnPress = false;
    // div().key_context(".."): the context a keystroke is resolved against
    // while focus is anywhere in this subtree, which a binding's predicate
    // reads — "Editor", or "Editor mode=full". Hashed, since that is all an
    // id is; KeyContextOf keeps the parse behind the hash.
    uint32_t keyContext = 0;
    // FocusableExt::focus_ring: whether a focused element shows the focus
    // appearance at all. Rust gates the whole `focus_ring_style` call on it,
    // so turning it off drops the tinted border along with the ring — a
    // control that draws its own focus some other way wants neither.
    bool focusRing = true;
    int trapId = 0;
    Str tooltip;
};

// One `on_action` handler. The tree is frame-arena, so a handful of these
// chained off an element costs a pointer each and dies with the frame.
struct ActionSlot {
    uint32_t action = 0;
    Listener fn = {};
    ActionSlot* next = nullptr;
};

// What an action handler is called with. Rust hands over the action itself,
// which carries whatever fields it was declared with; an action here is a
// name, so this is the name and the one thing a handler answers back.
struct ActionEvent {
    uint32_t action = 0;
    // What the action carries. Rust puts fields on the action type —
    // `Confirm { secondary }`, `SelectScrollbarMode(mode)`,
    // `MenuClick(name)` — and matches the whole value; an action here is the
    // hash of its name, and this is the rest of it. A number, a bool or an
    // enum is itself; anything larger is a pointer to something that outlives
    // the dispatch, which for a binding means a literal.
    intptr_t arg = 0;
    // cx.propagate(): the handler looked and did not want it, so the search
    // carries on outwards. Not setting it is Rust's default, which stops.
    bool propagate = false;
};

// text/state.rs SelectionFormat: what a copy of the window's selection says.
// Plain is the rendered text, which is all a run knows by itself. Source
// rebuilds the Markdown the run was rendered from, out of the affixes below.
enum class SelectionFormat : uint8_t {
    Plain = 0,
    Source
};

// The block a selectable run belongs to, for SelectionFormat::Source. Rust
// reconstructs a selection by walking the BlockNode tree it rendered from
// (node.rs `text_by_kind`); the window's selection here knows only the flat
// list of painted runs, so the tree's shape rides along on them. One record
// is shared by every run of a block, and `!=` is what says the selection has
// crossed into another one.
struct SelBlock {
    // Emitted when the selection first enters the block and when it leaves —
    // a code fence, a heading's `## `, a table row's leading `| `. `pre`
    // already carries `linePre`, so entering a block emits `pre` alone.
    Str pre;
    Str post;
    // Prefixed to every further line the block contributes, and to every
    // line break inside one of its runs: `> ` for a blockquote, the indent
    // under a list marker.
    Str linePre;
    // Whether entering this block continues the previous block's line rather
    // than starting a new one. Table cells in a row do.
    bool join = false;
};

// What SelectionFormat::Source needs from one selectable run: the marks
// around it, and the block it sits in.
//
// One record is shared by every adjacent run that carries the same marks, and
// the copier closes the group only when the record changes. That is what
// `reconstruct_markdown` does by walking mark *ranges* rather than words: a
// bold phrase split into three word elements has to copy as `**one two
// three**`, not as three wrapped words.
struct SelSource {
    // node.rs `wrap_with_mark`: the Markdown around the run's own text, in
    // the order that function nests it — code innermost, link outermost. A
    // partial selection still gets both halves, which is what Rust does with
    // a slice of a marked range.
    Str pre;
    Str post;
    // The block above. Null for a selectable run that is not Markdown, which
    // is every run outside a TextView.
    const SelBlock* block = nullptr;
};

struct FocusHandle {
    int id = 0;
    bool IsValid() const { return id != 0; }
    bool operator==(const FocusHandle& o) const { return id == o.id; }
    bool operator!=(const FocusHandle& o) const { return id != o.id; }
};

struct El {
    ElKind kind = ElKind::Div;
    // The frame arena this was built on, so a builder that has to allocate —
    // an action handler's slot — has one without being handed it again.
    Arena* arena = nullptr;
    Style style;
    Str id;
    Str text;
    IconName icon = IconName::None;
    Str iconPath;
    // ElKind::Image: what the document called the image. gpui/image.h says
    // what that may name.
    Str imgSrc;
    ChartSeries chart = {};
    float progress = 0; // 0..100
    int clickId = 0;
    // GlobalElementId. GPUI identifies an element by the *stack* of
    // ElementIds from the root down to it — `Window::with_id` pushes and pops
    // — so a name only has to be unique among its siblings, which is why
    // upstream can write `div().id(("showcase-tab", ix))` inside every tab
    // group without a thought for the one next door. There is no stack here:
    // an element is found by one flat int, so the path is folded into a hash
    // of it. `IdCollect` walks the built tree once a frame and fills this in;
    // an element with no name of its own inherits its parent's, exactly as an
    // element with no `.id()` pushes nothing in Rust.
    uint32_t pathId = 0;
    // El::PathId: the click id is the path rather than a number the caller
    // picked. An explicit Click(v) clears it and wins.
    bool clickFromPath = false;
    // El::StopClick — see HitRect::stopClick.
    bool stopClick = false;
    Func0 onClick;
    Listener listener;
    // El::OnClickAction — dispatched from the release, beside onClick.
    uint32_t clickAction = 0;
    intptr_t clickActionArg = 0;
    // `div().on_hover(..)`. Fires with a HoverEvent when the pointer enters
    // the element and again when it leaves, never in between.
    Listener onHover;
    Listener onScroll;
    ActionSlot* actions = nullptr;
    // window.on_mouse_event::<MouseDownEvent> bound to one element, which is
    // what `div().on_mouse_down(..)` is. A press runs this before the click
    // listener above; unlike the click, it carries the full MouseDownEvent.
    Listener onMouseDown;
    Listener onMouseUp;
    DispatchPhase mouseDownPhase = DispatchPhase::Bubble;
    DispatchPhase mouseUpPhase = DispatchPhase::Bubble;
    // on_drag_move. GPUI carries a drag entity so the move can name what is
    // being dragged; here the element that took the press keeps the moves
    // until the button comes back up, which is the same thing without the
    // entity. Needs a Click(id): the tree is rebuilt every frame, so the id
    // is what finds the element again.
    Listener onDragMove;
    // The refinement above, and the fields it names. Zero is no refinement.
    Style refine = {};
    uint32_t refineSet = 0;
    // `div().hover(|this| ..)` and `div().drag_over::<T>(|this, ..| ..)`:
    // refinements that hold only while the pointer is over the box, or while
    // a drag of `dragOverKind` is. Resolved where GPUI resolves them — in
    // `compute_style` during prepaint, against the hover the last frame left
    // — so unlike `HoverBg` they can name any field a refinement can, and a
    // caller no longer has to ask the window whether it is hovered and branch
    // on the answer while building.
    Style hoverStyle = {};
    uint32_t hoverSet = 0;
    Style dragOverStyle = {};
    uint32_t dragOverSet = 0;
    Str dragOverKind = {};
    // on_drag: what a press on this element picks up. The payload rides along
    // on every DragMoveEvent the drag produces.
    DragPayload drag = {};
    // div().cursor_col_resize() and friends: the shape the pointer takes over
    // this element. Rust hangs it off the style; it needs a Click(id) here for
    // the same reason a hover does, since the hit rect is what the move
    // consults.
    CursorKind cursor = CursorKind::Arrow;
    // on_mouse_up_out: a release that landed anywhere but on this element.
    // Rust hears it wherever the pointer is, whether or not the press started
    // here, and so does this.
    Listener onMouseUpOut;
    // on_drop::<T>(..) and drag_over::<T>(..): the kind of drag this element
    // takes, and what to do when one lets go over it. `WindowDragOverId` says
    // which element the drag is over right now, which is what a caller styles
    // on — GPUI applies `drag_over` itself because the style is part of the
    // element; an element here is rebuilt every frame and reads the answer
    // back instead.
    Str dropKind = {};
    Listener onDrop;
    // Where this element ended up, written at paint. GPUI's DockArea keeps
    // its own `bounds` the same way, through an element whose only job is to
    // report the box layout gave it; a caller that has to answer "what is
    // under the pointer" needs last frame's boxes to do it.
    gpui::Bounds* boundsOut = nullptr;
    // BindSlider: this element is a slider's track, and a press or a drag on
    // it moves that state. GPUI's slider elements capture the state entity in
    // their own closures; there are no closures on an element here, so the
    // element names the state and the window does what those closures do.
    SliderState* slider = nullptr;
    Axis sliderAxis = Axis::Horizontal;
    // BindInput: this element is a text field's editor box, so a press in it
    // places the caret and a drag extends the selection. The same trick as
    // BindSlider — Rust's InputElement captures the state entity in the
    // closures it installs, and an element here names the state instead.
    InputState* input = nullptr;
    // BindSliderBounds: this element is the rail a value maps against, and
    // hands its box to the state once it has one — SliderIndicator's
    // `on_prepaint(|bounds| state.set_bounds(bounds))`. The box that catches
    // the press is the taller track, so the two are not the same element.
    SliderState* sliderBounds = nullptr;
    void (*customPaint)(PaintCtx* ctx, El* e, void* user) = nullptr;
    void* customUser = nullptr;
    El* first = nullptr;
    El* last = nullptr;
    El* next = nullptr;
    float x = 0, y = 0, w = 0, h = 0;
    // The laid-out box as one value. The fields stay flat because the layout
    // pass writes them a component at a time. The return type is qualified
    // because this member hides `Bounds` inside El.
    gpui::Bounds Bounds() const { return {x, y, w, h}; }
    float scrollY = 0;
    // overflow_x_scroll: how far the content is slid to the left. Positive
    // means the view has moved right over it, as scrollY is positive-down.
    float scrollX = 0;
    // The bar this box shows, and whether the caller named it. Unnamed, it
    // is the theme's — Rust's Scrollbar reads `cx.theme().scrollbar_mode`
    // unless the caller passed one.
    ScrollbarMode scrollMode = ScrollbarMode::Always;
    bool scrollModeSet = false;
    // A box that scrolls without showing a bar. In Rust the scrolling
    // container and the Scrollbar are two elements, so a container with no
    // Scrollbar beside it simply has none; the box paints its own bar here,
    // and this is how it says not to — a tab bar scrolls, and shows nothing.
    bool noScrollbar = false;
    // The same, one axis at a time. Rust's `.scrollbar(&handle, axis)` adds a
    // bar layer per axis the ScrollbarAxis names, so a box can scroll both
    // ways and show a bar down one of them only; the box here paints its own
    // pair, and this is how it says which of the two to leave off.
    bool noScrollbarX = false;
    bool noScrollbarY = false;
    int scrollId = 0;
    // El::ScrollFromPath: the scroll handle's identity is the element's place
    // in the tree rather than a number the caller hashed. An explicit
    // ScrollId(v) clears it and wins.
    bool scrollFromPath = false;
    float contentW = 0;
    float contentH = 0;
    int selLo = -1; // UTF-8 offsets into text, -1 = none
    int selHi = -1;
    // The highlighted runs inside this text, in order. The array is the
    // caller's — the frame arena, in practice — and outlives the frame the
    // element was built in.
    const TextSpan* spans = nullptr;
    int nSpans = 0;
    // Washes under this run, painted where the selection quad is and before
    // the glyphs — which is what Rust's `layout_search_matches` builds paths
    // for. They are a second array rather than more `spans` because the span
    // painter partitions the text and so cannot take two runs over the same
    // bytes, and a search match sits over whatever the highlighter said.
    // Only `lo`, `hi` and `bg` are read.
    const TextSpan* washes = nullptr;
    int nWashes = 0;
    // Runs that are underlined and nothing else: a diagnostic's squiggle is a
    // HighlightStyle with only `underline` set, so it marks the text without
    // taking the colour the language gave it.
    const TextSpan* underlines = nullptr;
    int nUnderlines = 0;
    // RangeOut: where a run of this text landed, in window coordinates, for
    // a caller that has to hit-test against it later. `range_to_bounds` in
    // Rust, which the go-to-definition hitbox is inserted over. One box: a
    // symbol is on one row, and the first rect is that row's.
    int rangeOutLo = -1;
    int rangeOutHi = -1;
    gpui::Bounds* rangeOut = nullptr;
    float* caretOutX = nullptr;
    float* caretOutY = nullptr;
    Rgba selColor = Rgba8(0x6b, 0xb3, 0xf0, 90);
    // The input method's provisional run, underlined the way Rust gives the
    // marked range its own UnderlineStyle. Same offsets, same -1 for none.
    int markLo = -1;
    int markHi = -1;
    bool selectable = false;
    // What a copy of this run says, and whether it continues the run before
    // it on the same line. The record is owned by whoever built the element —
    // the frame arena, in practice — and is null on every run that is not
    // Markdown.
    const SelSource* selSrc = nullptr;
    bool selJoin = false;
    // The caret this run draws, as a UTF-8 offset into it; -1 for none. Rust's
    // InputElement measures cursor_bounds in prepaint and paints a quad there,
    // which is what this is — putting the bar between two text runs instead
    // would shift the glyphs by its own width every time it appeared.
    int caretOff = -1;
    Rgba caretColor = {};
    float caretW = 2;
    // The taffy node this element was laid out as, this frame. A
    // `taffy::NodeId` is a u64 and is kept as one here so gpui.h does not
    // have to name the layout port's types.
    uint64_t layoutNode = 0;
    float laidFont = 0; // resolved font size from last LayoutEl
    float laidMaxW = 0; // MeasureText maxW used (0 = unconstrained)
    // The shaped run LayoutEl measured, borrowed from the text cache so the
    // paint pass can draw it without looking it up a second time. Owned by
    // the cache, which cannot drop it before the frame ends; null when the
    // element has no text or the run could not be cached.
    TextLayout* laidLayout = nullptr;
    // What the measure callback last answered for this text leaf, keyed on
    // the width it was asked about. Taffy asks a leaf for its size several
    // times a pass — the min-content width, the max-content width, then the
    // width the line settled on — and each of those went all the way into the
    // shaped-run cache, which hashes the whole string and then memcmps it.
    // Font size, weight, wrap and line height are settled by PrepareEl before
    // layout starts and the element is built afresh every frame, so the width
    // is the whole key and nothing here outlives the text it measured.
    float measKeyW[4] = {};
    Size measSize[4] = {};
    uint8_t measCount = 0;
    uint8_t measNext = 0;

    // display: flex, leaving the direction at its row default — gpui's
    // `.flex()`, for a box that wants the flex model without saying which way
    // it runs.
    El* Flex();
    El* FlexRow();
    El* FlexCol();
    El* FlexRowReverse();
    El* FlexColReverse();
    El* FlexWrap();
    El* Grow(float g = 1);
    El* Shrink0();
    // flex_1(): grow 1, shrink 1, basis 0. The three together are what makes
    // siblings share a line evenly whatever is inside them, and grow alone
    // does not — with an auto basis each item keeps its content's width and
    // only the slack is split.
    El* Flex1();
    // flex_none(): neither grows nor shrinks, and keeps its own size.
    El* FlexNone();
    El* Basis(float v);
    // flex_basis(relative(f)): the main size a flex item starts from, as a
    // fraction of the line rather than a length. Siblings that all start
    // from the whole line and shrink together end up sharing it in the
    // proportions of their fractions, which is how a table row's cells are
    // sized.
    El* BasisFrac(float f);
    // flex_shrink(f). Shrink0() is the common case; this is the factor.
    El* Shrink(float f);
    El* W(float v);
    El* WFrac(float f);
    // percentage(delta) turns clockwise, which is what a spinner is made of.
    El* Rotate(float turns);
    // Scroll without a bar: the box still takes the wheel and still clips.
    El* HideScrollbar();
    // The bar down one axis only, for a box that scrolls both ways —
    // ScrollbarAxis::Vertical hides the horizontal one and the other way
    // round.
    El* HideScrollbarX();
    El* HideScrollbarY();
    // opacity(f): this element and everything under it, faded together.
    // Nested opacities multiply, as GPUI's do.
    El* Opacity(float f);
    El* H(float v);
    El* SizeFull();
    El* MinH(float v);
    El* MinW(float v);
    El* MaxW(float v);
    El* MaxH(float v);
    El* Gap(float v);
    El* GapX(float v);
    El* GapY(float v);
    El* Pad(float v);
    El* PadX(float v);
    El* PadY(float v);
    El* PadL(float v);
    El* PadR(float v);
    El* PadT(float v);
    El* PadB(float v);
    El* ItemsCenter();
    El* ItemsStart();
    El* ItemsEnd();
    El* ItemsStretch();
    El* JustifyBetween();
    El* JustifyAround();
    El* JustifyCenter();
    El* JustifyEnd();
    El* JustifyStart();
    El* Bg(Background c);
    El* Border(float width, Rgba c);
    El* BorderT(float width, Rgba c);
    El* BorderB(float width, Rgba c);
    El* BorderL(float width, Rgba c);
    El* BorderR(float width, Rgba c);
    El* Radius(float r);
    // rounded_tl / rounded_tr / rounded_br / rounded_bl, as one call. A corner
    // left at 0 is square, which is what `rounded_none` does to all four.
    El* Corners(float tl, float tr, float br, float bl);
    El* Fg(Rgba c);
    El* Font(float px);
    El* LineHeight(float mult);
    El* Truncate();
    El* ClipY();
    El* ScrollY(float off);
    El* ScrollX(float off);
    El* ClipX();
    El* ScrollMode(ScrollbarMode m);
    El* ScrollId(int v);
    El* Click(int v);
    // `div().id(name)` — the whole of it. The element is named, and the id it
    // is found by is that name folded with its ancestors'. This is what a
    // widget should reach for: two `Button::New(cx, StrL("save"))` under
    // different parents are two different elements, the way Rust's two
    // `Button::new("save")` are, and neither caller has to invent a name the
    // other will not also pick.
    El* PathId(Str name);
    // The same without joining the tab order, for a box that is a hit target
    // and nothing else.
    El* PathClick(Str name);
    // The name, and the focus id from the fold — for an element the keyboard
    // reaches without the pointer being able to press it.
    El* PathFocus(Str name);
    // `ScrollHandle::new()` kept on the view: what scrolls is identified by
    // which box it is, not by a name. The box already has a place in the
    // tree, so this takes the scroll id from the fold and leaves the name
    // alone — an element can be named for one thing and scroll as another.
    El* ScrollFromPath();
    El* OnClick(Func0 fn);
    El* OnClick(Listener l);
    // The scrollbar's own handler. Rust's scrollbar writes straight into the
    // shared ScrollHandle; here the view owns the offset, so dragging the
    // thumb or pressing the track reports one for it to store.
    El* OnScroll(Listener l);
    El* OnHover(Listener l);
    El* OnMouseDown(Listener l, DispatchPhase phase = DispatchPhase::Bubble);
    El* OnMouseUp(Listener l, DispatchPhase phase = DispatchPhase::Bubble);
    El* OnDragMove(Listener l);
    El* OnDrag(Str dragKind, int ix = 0, void* data = nullptr);
    El* OnMouseUpOut(Listener l);
    // cx.stop_propagation() for the click this element takes.
    El* StopClick();
    El* OnDrop(Str acceptKind, Listener l);
    // StyleRefinement, applied at layout time rather than as the caller
    // chains: a semantic state — selected, disabled — is meant to win over the
    // instance style underneath it, and the instance style is whatever the
    // caller chains onto the element after the primitive handed it back.
    El* Refine(const Style& s, uint32_t fields);
    // `div().hover(..)`. The refinement is a StateStyle, which is what every
    // other refinement in this tree is built with.
    El* Hover(const struct StateStyle& s);
    // `div().drag_over::<T>(..)`, where `kind` is the drag payload's kind the
    // way `OnDrop` names it.
    El* DragOver(Str dragKind, const struct StateStyle& s);
    El* BoundsOut(gpui::Bounds* out);
    El* Cursor(CursorKind c);
    El* BindSlider(SliderState* s, Axis axis = Axis::Horizontal);
    El* BindSliderBounds(SliderState* s);
    El* BindInput(InputState* s);
    // The selection quad and the caret an input's text run paints over itself.
    El* SelRange(int lo, int hi, Rgba color);
    // Where the caret this element draws ended up, in window coordinates —
    // the seam a popover anchored to the caret needs, since only the painter
    // knows where inside a shaped run an offset falls. Reported the way
    // BoundsOut reports a box: one frame stale, which is what every other
    // popover here is placed against.
    El* CaretOut(float* outX, float* outY);
    // Report where the bytes [lo, hi) of this run were painted.
    El* RangeOut(int lo, int hi, gpui::Bounds* out);
    El* Washes(const TextSpan* runs, int n);
    El* Underlines(const TextSpan* runs, int n);
    El* Spans(const TextSpan* runs, int n);
    // The marked range, which is drawn underlined in the text's own colour.
    El* MarkRange(int lo, int hi);
    El* Caret(int off, Rgba color, float width = 2);
    El* Child(El* c);
    El* Bold();
    El* Semibold();
    El* Medium();
    El* Mono();
    El* Underline();
    El* Strikethrough();
    El* Italic();
    El* Selectable();
    // The Markdown this run came from, and whether it continues the run
    // before it rather than starting a line of its own.
    El* SelSrc(const SelSource* s, bool join);
    El* Wrap();
    El* Dashed();
    El* DashArray(float on, float off);
    El* Absolute();
    El* Fixed();
    El* Deferred();
    El* AnchorBelow(float gap = 0);
    // `Positioner::side`: flip to the other side rather than clamping when
    // the anchored side has no room. Dropdowns say this; a popup placed at a
    // named corner does not.
    El* AnchorFlip(bool on = true);
    El* AnchorAbove(float gap = 0);
    El* AnchorCenterX();
    El* Top(float v);
    El* Left(float v);
    El* Bottom(float v);
    El* Right(float v);
    El* LeftRel(float frac);
    El* RightRel(float frac);
    El* HoverBg(Background c);
    El* HoverFg(Rgba c);
    // .active(|style| style.bg(..)): the fill while the box is held down. It
    // wins over the hover fill, the way Rust refines the active style over
    // the hovered one.
    El* ActiveBg(Background c);
    // div().group("") and .group_hover(..): the group, and a descendant that
    // only paints while the pointer is inside it.
    // The press-focus opt-in above.
    El* FocusOnPress(bool v = true);
    El* Group();
    El* GroupHoverVisible();
    // The other half of `group_hover`: a fill rather than a visibility, for a
    // strip that lights when the pointer is in the box around it.
    El* GroupHoverBg(Background c);
    El* FocusId(int v);
    // `div().track_focus(&handle)`. The element is focusable, and what it is
    // focusable *as* is the handle the caller's state owns rather than
    // anything derived from the element's name.
    El* TrackFocus(FocusHandle handle);
    El* KeyContext(Str name);
    // on_action::<A>(..). The listener is called with an ActionEvent; setting
    // its `propagate` passes the action on outwards, which is cx.propagate().
    El* OnAction(uint32_t action, Listener fn);
    // `.on_click(|_, window, cx| window.dispatch_action(Box::new(Cancel), cx))`
    // — the click runs whatever the keyboard's binding for that action runs,
    // rather than the caller passing the same handler to both. The dispatch
    // starts at the focused element, not at this one, which is what makes a
    // dialog's Cancel button and its escape key one handler.
    El* OnClickAction(uint32_t action, intptr_t arg = 0);
    // div().on_key_down(..): the raw keystroke, offered to the focused element
    // and then out through the elements above it, before the keymap resolves
    // the chord to an action. It is what a field that is not a text editor
    // reads — an OTP input takes a digit and a backspace and nothing else, so
    // there is no action to bind and no `InputState` to hand the window. Both
    // halves of a keystroke arrive here: the key itself, and the character it
    // produced, with `ch` set and `vk` zero.
    El* OnKeyDown(Listener fn);
    El* TabIndex(int v);
    El* TabStop(bool v);
    El* FocusRing(bool v);
    El* TrapId(int v);
    El* Tip(Str s);
    El* Id(Str s);
};

enum class BtnKind : uint8_t {
    Default,
    Primary,
    Outline
};

El* ButtonEl(Arena* a, int clickId, Str label, BtnKind kind = BtnKind::Default);
El* ButtonSmall(Arena* a, int clickId, Str label, BtnKind kind, bool selected);

El* Div(Arena* a);
El* TextEl(Arena* a, Str s);
El* IconEl(Arena* a, IconName name);
El* IconEl(Arena* a, IconName name, float size);
// gpui's img(src): `alt` is what paints when the source will not decode.
// The size is the image's own unless W() / H() overrides it, and it is
// scaled down to fit the width it is given — object_fit(Contain) with
// max_w(relative(1.)), which is how node.rs renders a markdown image.
El* ImageEl(Arena* a, Str src, Str alt = {});
El* ProgressEl(Arena* a, float value01to100, float barW, float barH);
El* ChartEl(Arena* a, const float* ys, int n, Rgba stroke, Rgba fillTop,
            Rgba fillBot, int tickMargin);

// ─── paint / window ───────────────────────────────────────────────────────

struct HitRect {
    // FocusHandle: what a press on this box focuses. Until handles existed
    // this was always `id` — every focusable box derived both numbers from
    // one name — and a box tracking a handle is the first case where the two
    // differ.
    int focusId = 0;
    int id = 0;
    Bounds bounds = {};
    Func0 onClick;
    Listener listener;
    Listener onHover;
    Listener onMouseDown;
    Listener onMouseUp;
    // Which pass of the chain each of the two was registered for.
    DispatchPhase mouseDownPhase = DispatchPhase::Bubble;
    DispatchPhase mouseUpPhase = DispatchPhase::Bubble;
    // The enclosing element that also recorded a hit rect, by index, or -1.
    // The chain a mouse event walks is this, not every box that happens to
    // contain the pointer — two absolutely placed siblings can overlap
    // without either being inside the other.
    int parent = -1;
    Listener onDragMove;
    DragPayload drag = {};
    Listener onMouseUpOut;
    Str dropKind = {};
    Listener onDrop;
    CursorKind cursor = CursorKind::Arrow;
    // El::Tip. The overlay reads it when the pointer arrives, so it has to
    // survive the hit test rather than only the paint that drew it.
    Str tooltip = {};
    SliderState* slider = nullptr;
    Axis sliderAxis = Axis::Horizontal;
    InputState* input = nullptr;
    // El::OnClickAction: the action a click dispatches, and what it carries.
    uint32_t clickAction = 0;
    intptr_t clickActionArg = 0;
    // El::StopClick: the click stops here rather than carrying on outwards.
    // `cx.stop_propagation()` in a handler, said on the element instead —
    // which is where a port whose listeners cannot wrap one another can say
    // it. A field's clear button is the case: pressing the × must not also be
    // a press on the trigger it sits in.
    bool stopClick = false;
};

// A scrolled box the frame painted, and the scrollbar drawn down its right
// edge. Rust's scrollbar reaches its ScrollHandle directly; the offset here
// belongs to whichever view passed it in through El::ScrollY, so a press on
// the bar reports the offset it computed and the view stores it.
struct ScrollRect {
    int id = 0;
    Bounds bounds = {};
    float contentH = 0;
    float scrollY = 0;
    float contentW = 0;
    float scrollX = 0;
    ScrollbarMode mode = ScrollbarMode::Always;
    // Which of the two bars this box shows, from El::HideScrollbar and its
    // per-axis pair: a bar that is not painted is not there to grab either.
    bool barX = true;
    bool barY = true;
    // Whether the bar is on screen at all. A faded-out bar keeps its layout
    // and its band, and takes no press: `tracks_thumb_hover` and the disabled
    // hitbox in scrollbar.rs say the same thing.
    bool barVisible = true;
    Listener onScroll;
    // The text field this box scrolls, when it is one. An InputState is not
    // an entity and so cannot be the target of a Listener; the element names
    // the state the way El::BindInput does, and the bar writes the offset
    // straight onto it.
    InputState* input = nullptr;
};

// The scrollbar as it is drawn. THUMB_WIDTH is the thin one a fading
// `Scrolling` bar rests at; THUMB_ACTIVE_WIDTH is what every other mode draws,
// and what any bar grows to under the pointer or in a drag. THUMB_INSET is the
// margin either side, so the band a press counts in — Rust's WIDTH, 4*2+8 — is
// the wide thumb plus both insets.
const float kScrollbarThumbW = 6.f;
const float kScrollbarThumbActiveW = 8.f;
const float kScrollbarThumbMargin = 4.f;
const float kScrollbarBandW =
    kScrollbarThumbActiveW + kScrollbarThumbMargin * 2.f;

// What El::OnScroll hands its handler: the box that was scrolled and where it
// should now be. Positive-down, as El::ScrollY takes it.
struct ScrollEvent {
    int id = 0;
    float offsetY = 0;
    // The horizontal offset, for a box that scrolls both ways. A handler that
    // only scrolls down can ignore it; it is whatever it already was.
    float offsetX = 0;
};

struct TextHit {
    Bounds bounds = {};
    Str text;
    float font = 14;
    float maxW = 0;
    bool wrap = false;
    int docOff = 0;
    // El::SelSrc, for a copy in SelectionFormat::Source. Null otherwise, and
    // then the run copies as its plain text in both formats.
    const SelSource* src = nullptr;
    // Whether this run continues the one before it on the same line. A
    // paragraph is one `InlineState.text` in Rust; here it is a row of word
    // elements, and this is what keeps a copy of it on one line — in both
    // formats, since that is the document's shape and not its syntax.
    bool join = false;
    // A run with no text of its own that copies as its `src->pre` alone: an
    // inline image, which node.rs gives no selection (`Paragraph::text`
    // concatenates the children's text and an image child has none) but whose
    // `![alt](url)` `selected_source` emits when the selection runs into it.
    // It holds a place in the document order so the selection can reach it,
    // and copies as nothing in Plain.
    bool atom = false;
    // TextSelectionScopeId: the focus trap this run sits inside, 0 for the
    // page itself. A selection belongs to one scope, so a drag that started
    // in a dialog does not run on into the page behind it.
    int scope = 0;
};

// Two-generation shaped-text cache (see TextMeas* in Gpui.cpp). Opaque slots.
struct TextMeasCache {
    void* slots = nullptr;
    int cap = 0;
    int used = 0;
    uint32_t frame = 0;
};

// The 2D backend. Direct2D + DirectWrite on Windows, cairo + Pango on Linux;
// both are opaque here and only Paint_win.cpp / Paint_linux.cpp look inside.
// `PaintApp` is the process-wide half (factories, fonts), `PaintTarget` the
// per-window drawing surface.
struct PaintApp;
struct PaintTarget;

// What the inspector is looking at. GPUI's Inspector picks an element out of
// the window and shows where it came from; an element here has no source
// location — nothing takes `#[track_caller]` — so what it can say for itself
// is its id, the box layout gave it, and the style it was built with.
// The style fields the inspector's editor can name. Rust serialises the whole
// `StyleRefinement` with serde and parses it back; there is no reflection
// table here, so this is the subset written out by hand — the ones the panel
// already reports, plus the two colours and the opacity beside them.
enum StyleField : uint32_t {
    StyleFieldBg = 1u << 0,
    StyleFieldColor = 1u << 1,
    StyleFieldBorderColor = 1u << 2,
    StyleFieldPad = 1u << 3,
    StyleFieldGap = 1u << 4,
    StyleFieldRadius = 1u << 5,
    StyleFieldBorder = 1u << 6,
    StyleFieldFontSize = 1u << 7,
    StyleFieldWidth = 1u << 8,
    StyleFieldHeight = 1u << 9,
    StyleFieldOpacity = 1u << 10,
    // Not fields the inspector's editor offers — a hover colour is not a
    // property of the box, it is what the box becomes under the pointer — but
    // they are fields a StyleRefinement can name, and `state_style.h` names
    // them.
    StyleFieldHoverBg = 1u << 11,
    StyleFieldHoverFg = 1u << 12,
    StyleFieldActiveBg = 1u << 13,
    // One side each, because Rust's `.border_l_2()` names the left edge and
    // leaves the other three alone — a refinement that copied all four would
    // clear whatever the box already had on them.
    StyleFieldBorderT = 1u << 14,
    StyleFieldBorderB = 1u << 15,
    StyleFieldBorderL = 1u << 16,
    StyleFieldBorderR = 1u << 17
};

// StyleRefinement::refine, over the fields `fields` names and no others. The
// inspector's live edit and a control's semantic state are both refinements
// of a whole style, so they go through the same place.
void StyleApplyFields(Style* into, const Style& over, uint32_t fields);

// A live style override, which is what Rust's DivInspector writes back into
// the `StyleRefinement` of the element it took over. The tree is rebuilt every
// frame and its `El`s go with it, so the override is keyed by the element's
// click id and applied on the way through layout. One table, since there is
// one inspector.
void StyleOverrideSet(int clickId, uint32_t fields, const Style& style);
void StyleOverrideClear(int clickId);
void StyleOverrideClearAll();
// Patches `e->style` in place with whatever is on file for its click id.
void StyleOverrideApply(El* e);

struct InspectorPick {
    int id = 0;
    Str elId = {};
    // The whole style the element was built with, which is what the editor
    // serialises and what Reset puts back.
    Style style = {};
    Bounds bounds = {};
    // The kind of element, as El::kind reads it.
    int kind = 0;
    int depth = 0;
    bool hasBg = false;
    Rgba bg = {};
    float pad = 0;
    float gap = 0;
    float radius = 0;
    float border = 0;
    bool row = true;
    float font = 0;
    // The text a Text element holds, so a picked label says which one it is.
    Str text = {};
};

// window.toggle_inspector / Inspector::is_picking. The panel is the caller's
// to render — `component::Inspector` is the one this tree ships — and this is
// the state it reads.
struct InspectorState {
    bool on = false;
    bool picking = false;
    bool hasPick = false;
    InspectorPick pick = {};
    // A press while picking names the element it landed on, but the frame
    // that painted last was aimed at wherever the pointer was then. The pick
    // is settled on the next frame instead, against the press itself.
    bool pending = false;
    float pendingX = 0;
    float pendingY = 0;
};

struct PaintCtx {
    PaintApp* pa = nullptr;
    PaintTarget* rt = nullptr;
    // Window::element_opacity: the Style::opacity of everything this element
    // is inside, multiplied together. Every colour painted is faded by it, so
    // a subtree fades as one thing rather than each of its boxes separately.
    float opacity = 1;
    float dpi = 96;
    float viewW = 0;
    float viewH = 0;
    int hoverId = 0;
    // Which drop target the pointer is over and what is being dragged, so a
    // `DragOver` refinement can be resolved beside the hover one. GPUI reads
    // the same pair off the window in `compute_style`.
    int dragOverId = 0;
    Str dragKind = {};
    // The element holding the press, which is what `Style::activeBg` is
    // matched against — GPUI's `clicked_state.element`. It is the id the
    // press landed on for as long as the button is down, and 0 otherwise,
    // so it does not follow the pointer the way hoverId does.
    int activeId = 0;
    // Whether the pointer is inside the nearest enclosing `Group()` box,
    // pushed down as the tree paints the way element opacity is.
    bool groupHovered = false;
    int focusId = 0;
    // window.focus_generation: bumped every time the focus moves, so a
    // keystroke can tell that it stayed put without holding onto the element.
    int focusGen = 0;
    // Where the pointer is, which is what a Hover-mode scrollbar consults.
    float mouseX = -1;
    float mouseY = -1;
    // The scrolled box whose bar is being dragged, and which of its two. A
    // dragged thumb wears the same appearance a hovered one does — Rust's
    // `dragged_axis` — and the pointer may be nowhere near it by then, so the
    // drag has to reach the paint pass rather than being inferred from where
    // the pointer is.
    int scrollDragId = 0;
    bool scrollDragHorizontal = false;
    // Something painted this frame is part-way through a transition and wants
    // the window back: a Scrolling scrollbar fading out. The window asks for
    // an animation frame once, after the tree has painted, rather than each
    // fading bar asking for itself.
    bool wantsAnimFrame = false;
    // The enclosing hit rect while the tree paints, which is what a hit rect
    // records as its parent.
    int hitParent = -1;
    // The inspector picking an element: every box under the pointer overwrites
    // this as it paints, so the deepest one wins — which is the one a click
    // would land on.
    bool picking = false;
    bool pickHit = false;
    // How deep in the tree the element being painted is, which is what the
    // pick prefers on: an overlay layer painted last is not the element under
    // the pointer just because it went down after everything else.
    int paintDepth = 0;
    // Which stacking layer the tree is painting in — the `kPaintLayer*`
    // constants below. GPUI's primitives carry the order of the stacking
    // context they were built in and the scene sorts on it; here the paint
    // walks already run in that order, and the field is what lets
    // src/gpui/scene.cpp record it rather than infer it.
    int paintLayer = 0;
    // How good a candidate the pick so far was: 2 for an element with an id,
    // 1 for one that draws something, so an unnamed label inside a button
    // does not stand in front of the button.
    int pickTier = 0;
    InspectorPick pick = {};
    Vec<HitRect> hits;
    Vec<ScrollRect> scrolls;
    Vec<TextHit> texts;
    // The fields this frame painted, outermost box first, so a press can find
    // the one it landed in. A hit rect would shadow the click on the box
    // around it; these are a list of their own for the same reason GPUI
    // installs the editor's mouse handlers beside the container's, not
    // instead of them.
    Vec<InputState*> inputs;
    int textDocLen = 0;
    int selA = -1;
    int selB = -1;
    // Which scope the range above belongs to; -1 paints it wherever it
    // falls, which is what a caller that knows of no scopes wants.
    int selScope = -1;
    TextMeasCache textCache;

    PaintCtx() = default;
};

struct FocusRect {
    int id = 0;
    int trapId = 0;
    int tabIndex = 0;
    bool tabStop = true;
    bool focusOnPress = false;
    // Where this element sits in the frame's dispatch list. Rust walks the
    // real tree to find what is above a focused handle; the tree here is gone
    // by the time a key arrives, so the walk is recorded while it is still
    // there — see DispatchNode.
    int dispatchIx = 0;
    Bounds bounds = {};
};

// One key context or one action handler, recorded in tree order. `subtreeEnd`
// is one past the last node of the element's whole subtree, so a node is
// above position `p` exactly when it was written before `p` and its subtree
// still has not closed: that is an ancestor test that does not care whether
// the elements in between contributed nodes of their own. Walking backwards
// from `p` visits them innermost first, which is the order Rust reads a
// dispatch path in.
struct DispatchNode {
    int subtreeEnd = 0;
    uint32_t context = 0;
    uint32_t action = 0;
    Listener fn = {};
};

// ─── UTF-8 scanning ───────────────────────────────────────────────────────
//
// What text_boundary.rs and the input engine both walk the text with. A byte
// that is not valid UTF-8 counts as one character of its own value: every
// caller only asks which class it lands in, and every stray byte lands in the
// same one.

// crates/base/src/text_boundary.rs CharacterKind.
enum class CharKind : uint8_t {
    Word,
    Whitespace,
    Newline,
    Other
};

// `CharKindOf` and `Utf8ClipLeft` are text_boundary.rs's own and live in
// `base/text_boundary.h`; these two are the `char` iteration Rust gets
// from `str`, which every caller of either needs first.

// The codepoint at byte `i` and how many bytes it took.
int Utf8At(Str s, int i, uint32_t* out);
// Where the character before `i` starts.
int Utf8Prev(Str s, int i);

// ─── rope ─────────────────────────────────────────────────────────────────
//
// crates/base/src/input/base/rope_ext.rs. Rust's input holds its document in a
// `ropey::Rope` and reaches it through the `RopeExt` trait; here the document
// is a flat UTF-8 buffer and the trait's methods are these functions over a
// `Str`. An input holds a form field or a page of code, not a file, so the
// piece table a rope buys is machinery with nothing to do.

// sum_tree::Bias: which side of a character an offset that lands inside one
// is pulled to.
enum class Bias : uint8_t {
    Left,
    Right
};

// rope_ext.rs Point — a row and a byte column inside it, not a position on
// screen. Named the way Rust names it; gpui::Point is the geometry one.
struct RopePoint {
    int row = 0;
    int column = 0;
};

// Into the text, then to a character boundary on the named side.
int RopeClipOffset(Str text, int offset, Bias bias);
// char_at: the codepoint at `offset`, and how many bytes it took. 0 when the
// offset is past the end — Rust's `None`.
int RopeCharAt(Str text, int offset, uint32_t* out);
int RopeLinesLen(Str text);
int RopeLineStartOffset(Str text, int row);
int RopeLineEndOffset(Str text, int row);
// slice_line: the row without its newline.
Str RopeSliceLine(Str text, int row);
int RopeLineLen(Str text, int row);
RopePoint RopeOffsetToPoint(Str text, int offset);
int RopePointToOffset(Str text, RopePoint point);
int RopeOffsetUtf16ToOffset(Str text, int offsetUtf16);
int RopeOffsetToOffsetUtf16(Str text, int offset);
int RopeCharIndexToOffset(Str text, int charIndex);
int RopeOffsetToCharIndex(Str text, int offset);

// ─── input ────────────────────────────────────────────────────────────────
//
// crates/base/src/input/base. Rust's engine is one `InputBaseState<M>`
// parameterized by a compile-time mode marker (`InputMode`, `TextareaMode`,
// `EditorMode`) so a method that only makes sense for one of them does not
// exist on the others. There are no traits to bound here, so the marker is a
// runtime `InputKind` and the methods that would not compile in Rust return
// early instead — `InputMoveVertical` on a single-line field, say.

// gpui_component::input::InputEvent.
enum class InputEventKind : uint8_t {
    Change,
    PressEnter,
    Focus,
    Blur
};

struct InputEvent {
    InputEventKind kind = InputEventKind::Change;
    // PressEnter { secondary, shift }.
    bool secondary = false;
    bool shift = false;
};

// cursor.rs Selection: a byte range into the text. Empty means the caret sits
// at `start`, with nothing selected.
struct Selection {
    int start = 0;
    int end = 0;

    int Len() const { return end > start ? end - start : 0; }
    bool IsEmpty() const { return start == end; }
    bool Contains(int offset) const { return offset >= start && offset < end; }
};

inline Selection SelectionAt(int offset) {
    return Selection{offset, offset};
}

// undo_manager.rs EditIntent. What kind of edit produced a change, which is
// what decides whether two of them coalesce into one undo step.
enum class EditIntent : uint8_t {
    Typing,
    Backspace,
    DeleteForward,
    Atomic
};

// change.rs Change. Rust's owns two `String`s; these are heap `Str`s the
// transaction that holds them frees.
struct Change {
    Selection oldRange = {};
    Str oldText = {};
    Selection newRange = {};
    Str newText = {};
    Selection selBefore = {};
    Selection selAfter = {};
};

// One undo step. Rust's holds a `Vec<Change>`; a `Vec<T>` here is memcpy-only
// and this lives inside another `Vec`, so the change list is a plain owned
// array the manager grows and frees by hand.
struct UndoTransaction {
    EditIntent intent = EditIntent::Atomic;
    Change* changes = nullptr;
    int len = 0;
    int cap = 0;
};

// undo_manager.rs UndoManager. Every edit makes a transaction; adjacent
// compatible ones coalesce until something breaks the run — a cursor move, a
// paste, a blur. IME composition brackets its callbacks with Begin/Commit.
struct UndoManager {
    Vec<UndoTransaction> undos;
    Vec<UndoTransaction> redos;
    bool ignoring = false;
    bool transactionOpen = false;
    bool hasPending = false;
    Change pending = {};
    // pending_intent: what the next replace should record itself as. Taken by
    // the edit that follows.
    bool hasPendingIntent = false;
    EditIntent pendingIntent = EditIntent::Atomic;
    bool coalescingBoundary = false;

    ~UndoManager();
};

void UndoRecordTransaction(UndoManager* m, Change change, EditIntent intent);
void UndoBeginTransaction(UndoManager* m);
void UndoCommitTransaction(UndoManager* m);
void UndoBreakCoalescing(UndoManager* m);
void UndoSetIgnoring(UndoManager* m, bool ignoring);
bool UndoIsIgnoring(const UndoManager* m);
void UndoClear(UndoManager* m);
// undo() / redo(). Rust clones the change list out; the transaction lives on
// the other stack either way, so these hand back the one that moved and the
// caller walks it — backwards for an undo, forwards for a redo. Null when the
// stack is empty.
const UndoTransaction* UndoPopUndo(UndoManager* m);
const UndoTransaction* UndoPopRedo(UndoManager* m);

// mask_pattern.rs MaskToken. `Sep` carries its character, which the pattern
// string holds, so this is the tag alone.
enum class MaskToken : uint8_t {
    Digit,         // 9  — [0-9]
    Letter,        // A  — [a-zA-Z]
    LetterOrDigit, // # — [a-zA-Z0-9]
    Any,           // *  — any character
    Sep            // anything else, matching only itself
};

enum class MaskKind : uint8_t {
    None,
    Pattern,
    Number
};

// mask_pattern.rs MaskPattern. Rust keeps the parsed `Vec<MaskToken>` beside
// the pattern; a token is a pure function of its character, so the pattern
// string is the whole state and `MaskTokenAt` reads it.
struct MaskPattern {
    MaskKind kind = MaskKind::None;
    // Pattern: owned, freed by MaskPatternFree.
    Str pattern = {};
    // Number: the group separator, 0 for None.
    uint32_t separator = 0;
    // Number: how many fraction digits to keep, -1 for None.
    int fraction = -1;
};

// `(999)999-9999`, `AAAA-99-####`, `*999*`.
MaskPattern MaskPatternNew(Str pattern);
MaskPattern MaskPatternNumber(uint32_t separator);
void MaskPatternFree(MaskPattern* p);
// The token at character index `pos`, and its separator character. False when
// the pattern has no token there.
bool MaskTokenAt(const MaskPattern& p, int pos, MaskToken* out, uint32_t* sep);
bool MaskIsNone(const MaskPattern& p);
bool MaskIsValid(const MaskPattern& p, Str maskText);
bool MaskIsValidAt(const MaskPattern& p, uint32_t ch, int pos);
// mask(): 123456789 through `(999)999-999` is `(123)456-789`.
Str MaskApply(Arena* a, const MaskPattern& p, Str text);
// unmask(): the original text back out of a masked one.
Str MaskUnapply(Arena* a, const MaskPattern& p, Str maskText);
// The cue a pattern shows when empty: `(___) ___-____`. Empty for the other
// two kinds, which is Rust's `None`.
Str MaskPlaceholder(Arena* a, const MaskPattern& p);
// normalize_number_input: full-width and CJK number characters folded to
// their ASCII equivalents, so `123。5` reaches the parser as `123.5`.
Str NormalizeNumberInput(Arena* a, Str text);

// kind.rs. Which of the three states this is. Rust fixes it at the type level;
// `InputIsMultiLine` is its `MULTI_LINE` associated constant.
enum class InputKind : uint8_t {
    Input,
    Textarea,
    Editor
};

// mode.rs LayoutMode. How the input lays its text out — rows and growth. Not
// the same question as `InputKind`: an auto-growing textarea capped at one row
// is still multi-line, which is the bug Rust split these two apart to fix.
enum class LayoutModeKind : uint8_t {
    PlainText,
    AutoGrow,
    CodeEditor
};

struct LayoutMode {
    LayoutModeKind kind = LayoutModeKind::PlainText;
    int rows = 1;
    int minRows = 1;
    int maxRows = 0; // 0 = usize::MAX
    int tabSize = 4;
    bool lineNumber = false;
    // LayoutMode::CodeEditor { folding }. Rust defaults it on and the story
    // turns it off; it is off here until something asks, because a plain
    // textarea has no gutter to hang the chevrons in.
    bool folding = false;
};

// LayoutMode::is_folding(): a code editor, with folding left on.
bool LayoutModeIsFolding(const LayoutMode& m);

void LayoutModeSetRows(LayoutMode* m, int rows);
int LayoutModeRows(const LayoutMode& m);
int LayoutModeMinRows(const LayoutMode& m);

// state.rs InputBaseState. The text a themed Input is bound to, everything
// ─── auto-scroll (crates/base/src/auto_scroll.rs) ─────────────────────────
//
// A drag that reaches the edge of a scrolling box keeps scrolling it while
// the pointer stays there, faster the further out it goes. Rust drives it
// with a 16 ms background task per state; here the frame loop is that clock,
// so what a state carries is only the delta in force and where the pointer
// was — the arithmetic is `AutoScrollComputeDelta` and is the same either way.
// It lives beside InputState because that is what holds one; the logic is in
// src/base/auto_scroll.cpp, the way the rest of the input engine is.

// MIN_SPEED and MAX_SPEED, in DIPs per tick.
const float kAutoScrollMinSpeed = 12.f;
const float kAutoScrollMaxSpeed = 64.f;
// INNER_ZONE: the trigger starts this far *inside* the box, so a drag still
// scrolls in a full-screen window where the pointer cannot get outside the
// element at all.
const float kAutoScrollInnerZone = 16.f;
// OUTER_RAMP: how far past the edge reaches MAX_SPEED. The ramp is the two
// added together, which is what makes one smooth curve with no flat part.
const float kAutoScrollOuterRamp = 80.f;

// compute_delta: how far a pointer at `y` should move the box, positive
// toward the bottom. False inside the dead zone, where nothing scrolls.
bool AutoScrollComputeDelta(float y, Bounds bounds, float* out);

struct AutoScroll {
    // The delta in force. Rust shares an `Option<Pixels>` with its task and
    // writes None to stop it; `active` is that None.
    float delta = 0;
    bool active = false;
    // last_drag_position: where the pointer was, so a tick can re-run the
    // selection at the same place while the content moves under it.
    Point lastDrag = {};
    bool hasLastDrag = false;

    bool IsActive() const { return active; }
    // set(Some(d)) and set(None). The second stops the ticking but keeps the
    // drag position, which is what a move back inside the box does.
    void Set(float d) {
        delta = d;
        active = true;
    }
    void SetNone() {
        delta = 0;
        active = false;
    }
    // stop(): the ticking and the drag position both.
    void Stop() {
        SetNone();
        lastDrag = {};
        hasLastDrag = false;
    }
};

// that edits it, and the undo history behind it. `onChange` is what Rust
// spells cx.subscribe(&input_state, |ev: &InputEvent| ...).
// SearchMatcher, crates/base/src/input/editor/search.rs: a query, where it is
// found in the text, and a cursor into that list. Rust builds an aho-corasick
// automaton over a single literal pattern, which is a substring scan with an
// ASCII case fold on the side — so that is what this is, and no library comes
// with it.
struct SearchMatcher {
    // matched_ranges, in order and non-overlapping.
    Vec<Selection> ranges;
    int current = 0;
    // ascii_case_insensitive, which is the fold aho-corasick is built with.
    bool caseInsensitive = true;
    // The query, owned. Empty is Rust's `None`: no automaton, no matches.
    Str query = {};
    // The text the ranges were found in. Rust clones the rope and compares
    // it to know whether anything moved; a clone is structural there and a
    // copy of the document here, which is the one cost this port pays for
    // keeping the comparison exact.
    Vec<char> text;
    // begin_replacement: set for the one update that a replacement causes, so
    // the cursor is clamped into the shorter list rather than reset to the
    // top. Cleared by that update, whether or not anything moved.
    bool replacing = false;

    ~SearchMatcher() {
        ranges.Reset();
        text.Reset();
        StrFree(query);
    }
};

// new(). A matcher is a plain value, so this is only for putting a used one
// back to the start.
void SearchMatcherReset(SearchMatcher* m);
// update(&text): the text is what it is now, and the matches follow.
void SearchMatcherUpdate(SearchMatcher* m, Str text);
void SearchMatcherUpdateQuery(SearchMatcher* m, Str query, bool insensitive);
inline int SearchMatcherLen(const SearchMatcher* m) {
    return m->ranges.len;
}
inline bool SearchMatcherIsEmpty(const SearchMatcher* m) {
    return m->ranges.len == 0;
}
inline int SearchMatcherIndex(const SearchMatcher* m) {
    return m->current;
}
// label(): "3/17", or "0/0" when nothing matched.
Str SearchMatcherLabel(Arena* a, const SearchMatcher* m);
// set_current_match_index: clamped into the list, as Rust's `.min(len - 1)`.
void SearchMatcherSetIndex(SearchMatcher* m, int ix);
void SearchMatcherBeginReplacement(SearchMatcher* m);
bool SearchMatcherHasNextWithoutWrap(const SearchMatcher* m);
// peek(): the range `next` would land on, without moving the cursor.
bool SearchMatcherPeek(const SearchMatcher* m, Selection* out);
// The current range, if there is one.
bool SearchMatcherCurrent(const SearchMatcher* m, Selection* out);
// update_cursor_by_offset: the first match at or after the offset, which is
// where a freshly opened panel starts from.
void SearchMatcherCursorByOffset(SearchMatcher* m, int offset);
// Iterator::next and DoubleEndedIterator::next_back, both of which wrap.
bool SearchMatcherNext(SearchMatcher* m, Selection* out);
bool SearchMatcherPrev(SearchMatcher* m, Selection* out);

/* Port of crates/base/src/input/editor/display_map — the folding half.

   Rust's display map is two projections stacked: buffer -> wrap (soft wrap)
   and wrap -> display (folding). The rows here are logical lines already — a
   soft-wrapped line is one row as tall as the text in it, so `rowBoxes` is
   indexed by line and the wrap projection has nowhere to live — which leaves
   the fold projection, and its two ends are line and display row rather than
   wrap row and display row. Everything else is `fold_map.rs` as written.

   Where the candidates come from is the themed layer's business, the way it
   is Rust's: `apply_highlighter_fold_candidates` takes whatever the
   highlighter found. */

// folding.rs FoldRange. A foldable run of lines, both ends inclusive.
struct FoldRange {
    int startLine = 0;
    int endLine = 0;
};

// fold_map.rs FoldMap. `candidates` is what could be folded and `folded` is
// what is; the two index vectors are the projection built from them, rebuilt
// lazily because a keystroke changes the text far more often than it changes
// which lines are hidden.
struct FoldMap {
    // Sorted by startLine, at most one range per startLine.
    Vec<FoldRange> candidates;
    // A subset of `candidates`, sorted the same way.
    Vec<FoldRange> folded;
    // display row -> line (fold_map's `visible_wrap_rows`).
    Vec<int> visibleLines;
    // line -> display row, -1 for a line inside a closed fold.
    Vec<int> lineToDisplayRow;
    bool needsRebuild = true;
    // The line count the projection was last built against, so a rebuild can
    // be skipped when neither the text nor the folds have moved.
    int cachedLineCount = 0;
};

// set_candidates: a full replacement. Sorts, drops all but the first range
// per start line, and forgets any fold whose candidate is gone.
void FoldMapSetCandidates(FoldMap* m, const FoldRange* ranges, int n);
// set_folded / toggle_fold. A start line that is not a candidate is ignored.
void FoldMapSetFolded(FoldMap* m, int startLine, bool folded);
void FoldMapToggle(FoldMap* m, int startLine);
bool FoldMapIsFolded(const FoldMap* m, int startLine);
bool FoldMapIsCandidate(const FoldMap* m, int startLine);
// clear_folds: everything opens, the candidates stay.
void FoldMapClearFolds(FoldMap* m);
// adjust_folds_for_edit: a fold or a candidate overlapping the edited lines
// is dropped, and one after them is shifted by however many lines the edit
// added or removed. Cheaper than re-extracting on every keystroke, and what
// keeps a fold attached to its text while it is typed above.
void FoldMapAdjustForEdit(FoldMap* m, int editStartLine, int editEndLine,
                          int lineDelta);
// rebuild: the projection, against a document of `lineCount` lines. A no-op
// unless something moved.
void FoldMapRebuild(FoldMap* m, int lineCount);
// How many rows are on screen — the line count when nothing is folded.
int FoldMapDisplayRowCount(const FoldMap* m);
// wrap_row_to_display_row / display_row_to_wrap_row, on lines. -1 for a line
// that is hidden, or a display row past the end.
int FoldMapDisplayRow(const FoldMap* m, int line);
int FoldMapLineAt(const FoldMap* m, int displayRow);
// True when a closed fold hides the line outright.
bool FoldMapLineHidden(const FoldMap* m, int line);
// nearest_visible_display_row, answered as a line: the line itself when it is
// visible, and the nearest one above it when it is not.
int FoldMapNearestVisibleLine(const FoldMap* m, int line);

// One chevron's box, from the frame the gutter last built. Rust inserts a
// hitbox per icon during prepaint and hangs a mouse-down listener on it; a
// press here is routed by the window through the field it landed in, so what
// the frame has to leave behind is where the icons were.
struct FoldIconBox {
    int line = 0;
    Bounds bounds = {};
};

// SearchSession: the panel's state, kept on the field so it survives the
// panel being closed and opened again.
struct SearchSession {
    bool open = false;
    bool replaceMode = false;
    bool caseInsensitive = true;
    Str query = {};       // owned
    Str replacement = {}; // owned
    // anchor_offset: where the view was when the panel opened, so the first
    // match chosen is the one nearest what you were looking at. -1 is None.
    int anchorOffset = -1;
    SearchMatcher matcher;

    ~SearchSession() {
        StrFree(query);
        StrFree(replacement);
    }
};

void SearchSessionSetQuery(SearchSession* s, Str query, bool insensitive);
void SearchSessionSetReplacement(SearchSession* s, Str replacement);

// input/editor/diagnostics.rs DiagnosticSeverity, in the order the colours
// are read by.
enum class DiagnosticSeverity : uint8_t {
    Hint,
    Error,
    Warning,
    Info
};

// One diagnostic over a range of the document. Rust keeps the LSP's own
// struct — related information, tags and a serde_json payload with it — and
// this keeps what an editor draws and says: where it is, how bad it is, what
// it says, and where it came from.
struct Diagnostic {
    Selection range = {};
    DiagnosticSeverity severity = DiagnosticSeverity::Info;
    Str message = {};
    Str source = {};
    Str code = {};
};

// lsp_types::CompletionItem, cut to what the menu shows and what accepting
// one writes. Rust carries the whole LSP struct — the sort text, the edits,
// the command that may follow — and the menu reads these five of it.
// lsp_types::TextEdit: a range of the document and what replaces it. Rust's
// range is a pair of positions; every seam in this tree speaks byte offsets,
// so this is the same edit written the way the rest of the input engine
// writes one.
struct TextEditItem {
    Selection range = {};
    Str newText = {};
};

// apply_lsp_edits: a list of them, in order. Each edit's range is resolved
// against the document *as the edits before it left it*, which is why a
// server sends them last-first — and each is its own undo step, which is what
// Rust's loop over `replace_text_in_range_silent` records (the `silent` there
// suppresses the completion trigger and says nothing about the history).
void InputApplyEdits(InputState* s, App* app, Window* win,
                     const TextEditItem* edits, int n);

struct CompletionItem {
    // What is shown, and what the query is matched against.
    Str label = {};
    // Shown muted and italic beside the label; the LSP's `detail`.
    Str detail = {};
    // What replaces the query. Empty means the label itself.
    Str insertText = {};
    // Markdown, in the pane beside the list.
    Str documentation = {};
    bool deprecated = false;
    // Whether `completionItem/resolve` has been asked about this item. An
    // item that came with documentation is never asked about.
    bool resolved = false;
    // `additionalTextEdits`: what else accepting this item writes — the
    // import a name needs, at the top of the document, while the name itself
    // goes in at the caret. Applied with the insert, as one undo step.
    const TextEditItem* additionalEdits = nullptr;
    int nAdditionalEdits = 0;
};

// CompletionProvider::completions, without the task: the provider is handed
// the document, where the caret is and the word being typed, and writes as
// many items as it has room for. Rust answers a future; there is nothing to
// await on here, so a provider that has to go somewhere slow does the going
// itself and answers what it has.
using CompletionFn = int (*)(void* data, Str text, int offset, Str query,
                             CompletionItem* out, int cap);

// ColorInformation: a range of the document that names a colour, and the
// colour it names — `#1e90ff`, `rgb(0 0 0)`, and what a provider makes of
// them. The range is painted in that colour behind the text it covers.
struct DocumentColor {
    Selection range = {};
    Rgba color = {};
};

// DocumentColorProvider::document_colors, without the task: the provider is
// handed the document and writes what it found, in document order.
using DocumentColorFn = int (*)(void* data, Str text, DocumentColor* out,
                                int cap);

// CodeAction, flattened: a title, and the one edit it makes — the range it
// replaces and what it puts there. Rust carries a WorkspaceEdit, which is a
// map of documents to edit lists; a field is one document and every action
// upstream writes makes one edit, so this is that edit.
struct CodeActionItem {
    Str title = {};
    // Which provider answered with it, so performing it goes back to that
    // one — Rust's `provider_id`.
    int provider = 0;
    Selection range = {};
    Str newText = {};
    // The whole edit list, for an action that is more than one. Rust carries
    // a WorkspaceEdit — a map of documents to edit lists — and a field is one
    // document, so an action is that document's list; the pair above is the
    // shorthand that every action upstream writes fits in, and is what is
    // used when this is empty.
    const TextEditItem* edits = nullptr;
    int nEdits = 0;
};

// CodeActionProvider::perform_code_action: the provider does the action
// itself rather than leaving its edits to the editor. True when it did.
struct CodeActionItem;
using CodeActionPerformFn = bool (*)(void* data, InputState* s, App* app,
                                     Window* win, const CodeActionItem* item);

// CodeActionProvider::code_actions, without the task: the provider is handed
// the document and what is selected, and writes the actions it offers there.
// Strings it answers are allocated out of `a`, which lives as long as the
// menu is up.
using CodeActionFn = int (*)(void* data, Arena* a, Str text, Selection sel,
                             CodeActionItem* out, int cap);

// The code action menu while it is up — CodeActionMenu's own state.
struct CodeActionSession {
    bool open = false;
    int selected = 0;
    Vec<CodeActionItem> items;
    // Bumped whenever the content changes. See CompletionSession::revision.
    uint64_t revision = 0;
    // What the titles and the replacement texts were written into, thrown
    // away and taken again each time the menu is asked for.
    Arena* arena = nullptr;

    ~CodeActionSession();
};

// HoverProvider::hover, without the task: the provider is handed the document
// and the offset the pointer is over, and answers the markdown to show, or an
// empty string for nothing to say. What it answers has to outlive the frame —
// a provider answers out of its own store, not off the stack.
using HoverFn = Str (*)(void* data, Str text, int offset);

// CompletionProvider::is_completion_trigger: whether what was just typed at
// that offset should open, keep or close the menu. Rust asks the provider on
// every keystroke; a provider that names none of this gets the rule below,
// which is what every provider in this tree has wanted.
enum class CompletionTrigger : uint8_t {
    // A word character: carry a menu that is up, open one that is not.
    Continue,
    // `.` and the like: open one where the caret stands, whatever is behind
    // it.
    Open,
    // Anything else, which puts the menu away.
    Close
};

using CompletionTriggerFn = CompletionTrigger (*)(void* data, Str text,
                                                  int offset, Str typed);

// CompletionProvider::resolve_completions — `completionItem/resolve`. The
// menu asks about the item the selection is on, once, when it arrived with
// no documentation of its own: a server that sends a thousand items sends
// them thin and fills one in when it is looked at. What it answers is
// allocated out of `a`, which lives as long as the menu.
using CompletionResolveFn = Str (*)(void* data, Arena* a,
                                    const CompletionItem* item);

// CompletionMenuOptions: how wide the popover may be. 320 is Rust's default,
// "fine for most identifiers"; a host that surfaces longer labels widens it.
const float kCompletionMenuMaxW = 320.f;

// CompletionProvider::inline_completion, without the task: the provider is
// handed the document and the caret and answers the text to suggest after it,
// or nothing. `textDocument/inlineCompletion` — the ghost text a suggestion
// engine puts in front of the caret, which Tab accepts. What it answers is
// allocated out of `a`, which lives until the suggestion is dropped.
using InlineCompletionFn = Str (*)(void* data, Arena* a, Str text, int offset);

// DEFAULT_INLINE_COMPLETION_DEBOUNCE: how long the typing has to stop before
// the provider is asked.
const float kInlineCompletionDebounceMs = 300.f;

// The suggestion in front of the caret, while there is one.
struct InlineCompletion {
    // What the provider answered, and where the caret was when it did. A
    // caret that has moved since drops it.
    Str text = {};
    int at = -1;
    // When the provider may be asked, and whether it has been. Rust spawns a
    // task with a timer; the frame is the clock here, the way every other
    // delay in this tree is.
    double dueAt = 0;
    bool asked = true;
    Arena* arena = nullptr;

    ~InlineCompletion();
};

// ─── range semantic tokens (input/editor/lsp/semantic_tokens.rs) ─────────
//
// The highlighting a language server publishes, layered over the built-in
// highlighter rather than replacing it. A token arrives delta-encoded — five
// numbers, each position relative to the token before it — and names its type
// by an index into a legend the provider declares. The *name* is what is
// cached: the colour is resolved from it at paint, so a theme change recolours
// with nothing refetched.

// One token as the wire carries it.
struct SemanticToken {
    uint32_t deltaLine = 0;
    uint32_t deltaStart = 0;
    uint32_t length = 0;
    uint32_t tokenType = 0;
    uint32_t tokenModifiers = 0;
};

// One decoded token: where it is in line and column — a token never spans a
// line — and the legend name of its type.
struct SemanticSpan {
    int line = 0;
    int col = 0;
    int len = 0;
    Str name = {};
};

// A decoded token resolved against a document: the bytes it covers, and the
// name a caller looks a colour up by.
struct SemanticRange {
    Selection range = {};
    Str name = {};
};

// DocumentRangeSemanticTokensProvider::semantic_tokens, without the task: the
// provider is handed the document and a byte range and writes the tokens in
// it, delta-encoded as a server would send them. The legend beside it is
// `DocumentRangeSemanticTokensProvider::legend`.
using SemanticTokensFn = int (*)(void* data, Str text, Selection range,
                                 SemanticToken* out, int cap);

// decode_semantic_tokens: the delta encoding unpacked into absolute
// positions, in document order. A token whose type is not in the legend is
// skipped, which is what an out-of-range index means.
int SemanticTokensDecode(const SemanticToken* toks, int n, const Str* names,
                         int nNames, SemanticSpan* out, int cap);

// semantic_tokens_for_range: the tokens touching `visible`, as byte ranges.
// The cache is in document order, so the window is binary-searched rather
// than scanned — a document of ten thousand tokens pays for the ones on
// screen. A token resolving to an empty range is skipped.
int SemanticTokensForRange(const SemanticSpan* toks, int n, Str text,
                           Selection visible, SemanticRange* out, int cap);

// LocationLink, flattened. `uri` is the document the target is in: empty
// means this one, and the target range is then an offset pair into the text
// being edited. An `http`/`https` uri is a page rather than a document, and
// goes to whatever the desktop opens links with.
struct DefinitionLink {
    // origin_selection_range: the symbol that was asked about. Empty lets the
    // word under the offset stand for it, which is what Rust falls back to.
    Selection origin = {};
    Str uri = {};
    // target_selection_range: what to select once we are there.
    Selection target = {};
};

// DefinitionProvider::definitions, without the task: the provider is handed
// the document and the offset, and writes the places that offset is defined
// in. Strings it answers are allocated out of `a`, which lives until the next
// question is asked.
using DefinitionFn = int (*)(void* data, Arena* a, Str text, int offset,
                             DefinitionLink* out, int cap);

// ShowDocumentHandler: the host's chance to show a document itself, which is
// the `window/showDocument` request. True means it did and the built-in
// handling is skipped — an external uri going to the browser, anything else
// jumping inside this document.
using ShowDocumentFn = bool (*)(void* data, Str uri, bool external,
                                Selection selection);

// HoverDefinition: what a secondary-hover found under the pointer, and what
// it found last. The last pair is what the GoToDefinition action goes by:
// the hover clears as soon as the modifier comes up, and the action still has
// to know what the symbol under the caret was.
struct HoverDefinition {
    Selection symbolRange = {};
    Vec<DefinitionLink> locations;
    Selection lastRange = {};
    Vec<DefinitionLink> lastLocations;
    // Where the symbol was last painted, in window coordinates — Rust inserts
    // a hitbox over exactly this, to put the hand cursor on it.
    Bounds bounds = {};
    // What the uris were written into, taken again each time the provider is
    // asked.
    Arena* arena = nullptr;

    ~HoverDefinition();
};

// The completion menu while it is up — CompletionMenu's own state.
struct CompletionSession {
    bool open = false;
    // Where the word being completed began, and the caret it was asked at.
    int triggerStart = -1;
    int offset = 0;
    int selected = 0;
    // What the provider answered, in its own order.
    Vec<CompletionItem> items;
    // Bumped whenever the content changes. A renderer that mirrors this menu
    // — a host drawing its own popover — compares revisions to decide whether
    // to rebuild, so it never has to compare the item list itself.
    uint64_t revision = 0;
    // What `completionItem/resolve` wrote into, dropped with the menu.
    Arena* arena = nullptr;

    ~CompletionSession();
};

// EditorStyle::diagnostics: the colour a severity underlines in.
struct DiagnosticColors {
    Rgba error = {};
    Rgba warning = {};
    Rgba info = {};
    Rgba hint = {};
};
// Declared here because the overlay hook below names it and the state that
// holds the hook comes before the action table.
enum class InputAction : uint8_t;

enum class InputOverlayKind : uint8_t {
    Completion,
    CodeAction
};

// set_overlay_action_handler: asked before the editor's own menu handling,
// with the menu that is open and the action that arrived. True means the host
// took it.
using OverlayActionFn = bool (*)(void* data, InputOverlayKind kind,
                                 InputAction action);

struct InputState {
    InputKind kind = InputKind::Input;
    LayoutMode mode = {};
    // Rust's `Rope`. NUL-terminated past `len` so a `const char*` reader still
    // works; the terminator is not counted.
    Vec<char> text;
    Selection selectedRange = {};
    bool selectionReversed = false;
    // selected_word_range: what a double click took, kept so dragging out of
    // it cannot shrink back inside the word.
    bool hasSelectedWordRange = false;
    Selection selectedWordRange = {};
    UndoManager undo;
    MaskPattern maskPattern = {};
    bool maskPatternSet = false;
    Str placeholder = {}; // owned
    bool focused = false;
    // The window this field is the focused one of, so that a field taken out
    // of the tree while focused can take its registration with it. Rust drops
    // a stale one lazily — `focused_input` checks the handle is still focused
    // — which needs a handle that outlives the view; here the state *is* the
    // registration, so it clears itself when it goes.
    Window* focusWin = nullptr;
    bool disabled = false;
    bool readonly = false;
    bool loading = false;
    // A masked field draws one bullet per character. InputMode only.
    bool masked = false;
    bool cleanOnEscape = false;
    bool submitOnEnter = false;
    // searchable / replaceable: whether ctrl-f opens a find bar over this
    // field at all, and whether that bar may write back. Rust defaults the
    // first to false and turns it on for the code editor, and the second to
    // true — a field that cannot be edited is not replaceable anyway, which
    // `InputIsReplaceable` is what says.
    bool searchable = false;
    bool replaceable = true;
    SearchSession search;
    // Code folding. The projection survives an edit; the icon boxes are the
    // last frame's and are rebuilt with it.
    FoldMap folds;
    Vec<FoldIconBox> foldIcons;
    // The line-number cell of the first row, which is what says where the
    // gutter is. Rust hangs a hitbox over the whole column and shows the
    // chevrons while it is hovered; the column is the same x for every row,
    // so one row's cell locates it.
    Bounds gutterBox = {};
    bool softWrap = true;
    // text_align: 0 left, 1 center, 2 right.
    int align = 0;
    // A press is down and every move until the release extends the selection.
    bool selecting = false;
    // A selection drag that has reached the edge of a scrolled field keeps
    // scrolling it. Single-line fields have nowhere to go and never set it.
    AutoScroll autoScroll;
    // This field's caret clock, InputState::blink_cursor. Created on first
    // use, so an InputState stays a plain value.
    EntityId blink = {};
    Listener onChange = {};
    // validate: `Fn(&str, &mut App) -> bool`. A plain function pointer plus
    // its captured value, the way Listener carries one.
    bool (*validate)(Str text, intptr_t arg) = nullptr;
    intptr_t validateArg = 0;
    // The text run the element last painted, so a press can be turned into an
    // offset. Rust keeps `last_bounds` + `last_layout` for the same reason;
    // in a multi-line field this is the *first* row, and the ones under it are
    // found by stepping `lastLineH` down from it — unless soft wrap made them
    // different heights, which is what `rowBoxes` is for.
    Bounds lastBounds = {};
    float lastFont = 0;
    float lastLineH = 0;
    // Whether those rows were drawn in the monospace family, so a press is
    // measured against the same advances they were laid out with.
    bool lastMono = false;
    // display_map.rs: the box each logical line was last laid out in. Soft
    // wrap makes them uneven — a line that wrapped is two of those boxes tall
    // or more — so a press cannot be turned into a row by arithmetic, and
    // neither can the caret's y. Empty when nothing wrapped, where the
    // arithmetic is right and cheaper. Sized before the rows are built, so
    // the pointers the elements are handed stay put for the frame.
    Vec<Bounds> rowBoxes;
    // DiagnosticSet: what a provider published over this document, in
    // document order. Rust keeps a SumTree so a range query is a seek; there
    // are tens of these on a screen, so this is the flat list the painter and
    // the hover both walk.
    Vec<Diagnostic> diagnostics;
    // The one the pointer is over, and where it was — `state.diagnostic_
    // popover()` in Rust, which the overlay turns into a popover. -1 is none.
    int hoverDiagnostic = -1;
    float hoverDiagnosticX = 0;
    float hoverDiagnosticY = 0;
    // The symbol the pointer is resting on, and who is asked about it —
    // `hover_popover` in Rust. The range is the word it was asked for, which
    // is what keeps it from asking again while the pointer stays inside it.
    HoverFn hoverProvider = nullptr;
    void* hoverData = nullptr;
    Str hoverText = {};
    Selection hoverRange = {};
    float hoverX = 0;
    float hoverY = 0;
    // The 150 ms Rust waits before asking, and only when nothing is showing:
    // a popover already up moves from word to word with no delay at all,
    // which is `should_delay = hover_popover.is_none()`. A frame is the clock
    // here, so this is when the question may be asked and what it is about.
    double hoverDueAt = 0;
    Selection hoverPending = {};
    bool hoverAsked = true;
    // The colours a provider found in the document, and who is asked. Rust
    // asks on a timer after every edit and diffs the answer; this asks the
    // frame after the edit, which is the same answer a frame sooner.
    DocumentColorFn documentColorProvider = nullptr;
    void* documentColorData = nullptr;
    Vec<DocumentColor> documentColors;
    bool documentColorsDirty = true;
    // The code action menu, and who fills it — cmd-. / ctrl-. asks whatever
    // is selected. Rust asks every registered provider and puts the answers
    // in one list; there is one here, and an example that wants two answers
    // both from the one it registers.
    CodeActionSession codeActions;
    // Rust holds a `Vec<Rc<dyn CodeActionProvider>>` and asks every one of
    // them, putting the answers in one list; each item remembers which
    // provider it came from so performing it goes back to that one. The same
    // here, with a small fixed table — a field with more than four sources of
    // code actions is not a case upstream has either.
    static const int kMaxCodeActionProviders = 4;
    CodeActionFn codeActionProviders[kMaxCodeActionProviders] = {};
    void* codeActionDatas[kMaxCodeActionProviders] = {};
    // perform_code_action: the provider does it, if it wants to. A provider
    // that says nothing has its edits applied by the editor, which is what
    // every action in this tree writes.
    CodeActionPerformFn codeActionPerform[kMaxCodeActionProviders] = {};
    int nCodeActionProviders = 0;
    // The first slot, under the name the one-provider callers already use.
    // Writing it is the same as registering one provider.
    CodeActionFn codeActionProvider = nullptr;
    void* codeActionData = nullptr;
    // The inline suggestion in front of the caret, and who is asked for one.
    // A field with no provider never shows one.
    InlineCompletionFn inlineCompletionProvider = nullptr;
    void* inlineCompletionData = nullptr;
    InlineCompletion inlineCompletion;
    // The semantic tokens a provider published, and who is asked. Rust
    // debounces the request 100 ms after an edit and diffs the answer; this
    // asks the frame after the edit, like the document colours beside it.
    SemanticTokensFn semanticTokensProvider = nullptr;
    void* semanticTokensData = nullptr;
    // The legend the provider's `token_type` indexes into. Rust asks the
    // provider for it every time; a provider here declares it once beside
    // the function.
    const Str* semanticLegend = nullptr;
    int nSemanticLegend = 0;
    Vec<SemanticSpan> semanticTokens;
    bool semanticTokensDirty = true;
    // Go to definition: who is asked, what the last question found, and the
    // host's hook for showing a document itself. A state with no provider
    // never underlines anything and never answers the action.
    DefinitionFn definitionProvider = nullptr;
    void* definitionData = nullptr;
    ShowDocumentFn showDocument = nullptr;
    void* showDocumentData = nullptr;
    HoverDefinition hoverDef;
    // The completion menu, and who fills it. A state with no provider never
    // opens one, which is every field that is not a code editor.
    CompletionSession completion;
    CompletionFn completionProvider = nullptr;
    void* completionData = nullptr;
    // set_overlay_action_handler: a host drawing its own popover takes the
    // keys before the editor's own menu does.
    OverlayActionFn overlayAction = nullptr;
    void* overlayActionData = nullptr;
    // completion_inserting / silent_replace_text: an edit the editor made on
    // the reader's behalf — accepting an item, performing an action — is not
    // typing, so it does not open a menu or ask for a suggestion.
    bool silentReplace = false;
    // is_completion_trigger and completionItem/resolve, both optional: the
    // built-in rule and no resolving are what a provider that names neither
    // gets.
    CompletionTriggerFn completionTrigger = nullptr;
    CompletionResolveFn completionResolve = nullptr;
    // CompletionMenuOptions::max_width, which the menu is drawn to.
    float completionMenuMaxW = kCompletionMenuMaxW;
    // The box the rows were laid out in as a whole, which is the scrolled
    // height once soft wrap has had its say.
    Bounds contentBox = {};
    // input_bounds: the whole field, what a press outside the run maps against.
    Bounds inputBounds = {};
    // scroll_handle: how far the field has scrolled under its own box, and
    // the box it scrolls inside. Positive-down, as El::ScrollY takes it; Rust
    // keeps the same pair, negative, on a ScrollHandle.
    float scrollX = 0;
    float scrollY = 0;
    float viewW = 0;
    float viewH = 0;
    // Where the caret was last painted, in window coordinates. `cursor_
    // layout()` in Rust, which is what a completion menu and a hover popover
    // are placed under.
    float caretWinX = 0;
    float caretWinY = 0;
    // The caret's x inside the run, measured when it was last painted, and
    // the whole scrolled height. `last_layout` is what Rust reads them off.
    float caretX = 0;
    float contentW = 0;
    float contentH = 0;
    // Suppressed while set_value writes the text, so a programmatic write is
    // not reported as the user having typed.
    bool emitEvents = true;
    // ime_marked_range: the text the input method has put in provisionally,
    // which is the document's until the composition commits or is abandoned.
    // Rust keeps an Option; `imeMarking` is the Some.
    Selection imeMarked = {};
    bool imeMarking = false;
    // preferred_column: the column a vertical move aims for, so walking down
    // past a short line and back up returns to where it started. -1 for none.
    int preferredColumn = -1;
    // The x Rust remembers beside it, which is what a walk over *display*
    // rows aims at — a column means nothing halfway through a wrapped line.
    // -1 for none; cleared by every move that is not part of the walk.
    float preferredX = -1;

    ~InputState();
};

// Which way a move went, which decides whether scroll_to may pull the view
// back the other way. Rust's MoveDirection.
enum class InputMoveDir : uint8_t {
    None,
    Up,
    Down
};

// scroll_to: the offset that brings the caret into view, from where the field
// is now. `caretY` is the top of the caret's line and `caretX` its position
// across the run; the answer keeps the caret a line's clearance from either
// edge and never scrolls past the content. A move that went up will not be
// answered with a downward scroll, and the other way about — Rust clamps the
// same way, so a vertical walk does not fight itself.
void InputScrollToCaret(InputState* s, float caretX, float caretY,
                        InputMoveDir dir);
// A negative `caretX` leaves the sideways offset alone, which is what
// scrolling to something that is not the caret wants: a search match is a
// row to bring into view, and how far across it sits is not measurable
// outside a paint.
void InputScrollToOffset(InputState* s, int offset, InputMoveDir dir);
// The same, for wherever the caret is now: the row it is on and the x the
// last paint measured.
void InputScrollToCursor(InputState* s, InputMoveDir dir);

// value() / the NUL-terminated view of it. Neither allocates.
Str InputValue(const InputState* s);
const char* InputCStr(const InputState* s);
// unmask_value(): the text with the mask's separators taken back out.
Str InputUnmaskValue(Arena* a, const InputState* s);
// selected_text().
Str InputSelectedValue(const InputState* s);
bool InputIsMultiLine(const InputState* s);
bool InputIsSingleLine(const InputState* s);
bool InputIsEditable(const InputState* s);
// is_copyable: whether the selection may leave the field. A masked one may
// not — what it shows is not what it holds, and a copy or a cut would put
// what it holds on the clipboard.
bool InputIsCopyable(const InputState* s);
// cursor(): the caret offset, which end of the selection depends on which way
// it was dragged.
int InputCursor(const InputState* s);
// cursor_position(): the row and column the caret is on.
RopePoint InputCursorPosition(const InputState* s);

// set_value(): replaces the text, resets the selection to the end, and clears
// the undo history — the programmatic write, not an edit.
void InputSetValue(InputState* s, Str value);
// replace_all(): the same replacement, but recorded so it can be undone.
void InputReplaceAll(InputState* s, App* app, Window* win, Str value);
void InputSetPlaceholder(InputState* s, Str value);
void InputSetMaskPattern(InputState* s, MaskPattern pattern);
// clean(): empties the field.
void InputClean(InputState* s, App* app, Window* win);
// insert() / replace(): a programmatic edit, recorded as one atomic step.
void InputInsert(InputState* s, App* app, Window* win, Str value);

// previous_boundary / next_boundary: one character either way.
int InputPreviousBoundary(const InputState* s, int offset);
int InputNextBoundary(const InputState* s, int offset);
// start_of_line / end_of_line, and the two word boundaries movement.rs asks
// for. A single-line field answers 0 and len for the first pair, as Rust does.
// A code editor that soft-wraps answers the *visual* row's ends first and the
// logical line's on a second press, which is what `soft_wrap &&
// is_code_editor()` gates in Rust; the window is what the wrapped row is
// measured against, and without one the answer is the logical line.
int InputStartOfLine(const InputState* s, Window* win = nullptr);
int InputEndOfLine(const InputState* s, Window* win = nullptr);
int InputPreviousStartOfWord(const InputState* s);
int InputNextEndOfWord(const InputState* s);

// move_to(): drops the selection and puts the caret at `offset`.
void InputMoveTo(InputState* s, App* app, Window* win, int offset);
// select_to(): drags the live end of the selection to `offset`.
void InputSelectTo(InputState* s, App* app, Window* win, int offset);
void InputSelectAll(InputState* s, App* app, Window* win);
void InputUnselect(InputState* s, App* app, Window* win);
void InputSetSelectedRange(InputState* s, App* app, Window* win, int a, int b);
// selection.rs: what a double and a triple click take.
void InputSelectWord(InputState* s, App* app, Window* win, int offset);
void InputSelectLine(InputState* s, App* app, Window* win, int offset);

// The actions state.rs binds, one per `impl` method there. The window turns a
// key chord into one of these with InputActionForKey and hands it over — which
// is what GPUI's action dispatch does for the focused element.
enum class InputAction : uint8_t {
    None,
    MoveLeft,
    MoveRight,
    MoveUp,
    MoveDown,
    MoveHome,
    MoveEnd,
    MoveToStart,
    MoveToEnd,
    MoveToPreviousWord,
    MoveToNextWord,
    MovePageUp,
    MovePageDown,
    SelectLeft,
    SelectRight,
    SelectUp,
    SelectDown,
    SelectAll,
    SelectToStart,
    SelectToEnd,
    SelectToStartOfLine,
    SelectToEndOfLine,
    SelectToPreviousWordStart,
    SelectToNextWordEnd,
    Backspace,
    Delete,
    DeleteToBeginningOfLine,
    DeleteToEndOfLine,
    DeleteToPreviousWordStart,
    DeleteToNextWordEnd,
    Enter,
    Escape,
    // indent.rs IndentInline / OutdentInline, which is what tab and shift-tab
    // are bound to inside a field. A single-line input, or one whose layout
    // has nothing to indent, does not handle them — that is Rust's
    // cx.propagate(), and here it is `false` back out of InputPerform, which
    // leaves the keystroke to the window's focus ring.
    IndentInline,
    OutdentInline,
    // The block pair, ctrl-] / ctrl-[. They work on whole lines: a caret
    // sitting halfway along one still moves the line, where the inline pair
    // would have put the tab where the caret is.
    Indent,
    Outdent,
    Copy,
    Cut,
    Paste,
    Undo,
    Redo,
    // ctrl-f and ctrl-h, which open the find bar over the field — the second
    // with its replace row already out. Rust binds both in the input's key
    // context and both do nothing on a field that is not searchable.
    Search,
    Replace,
    // cmd-. / ctrl-.: the code action menu over whatever is selected.
    ToggleCodeActions
};

// `platform` is Command on macOS and the Windows key elsewhere;
// `KeySecondary(ctrl, platform)` is the shortcut modifier the copy, paste and
// undo chords are written with.
InputAction InputActionForKey(const InputState* s, int vk, bool shift,
                              bool ctrl, bool alt, bool platform = false);
// True when the input consumed it, so the window does not also treat Enter as
// a click on the focused element. `shift` is Enter's modifier, which decides
// whether a submit-on-enter textarea inserts a newline.
bool InputPerform(InputState* s, App* app, Window* win, InputAction action,
                  bool shift);

// ─── completion ───────────────────────────────────────────────────────────

// The word being typed in front of the caret: where it starts, and what it
// is. A caret that is not after a word answers an empty query starting where
// it stands, which is what a trigger character like `.` completes on.
Str InputCompletionQuery(const InputState* s, int* startOut);
// Ask the provider and open the menu if it answered anything. Rust does this
// from the editor's own `on_input` when the typed character is a trigger, and
// from ctrl-space; `force` is the second, which asks whatever was typed.
void InputRequestCompletion(InputState* s, App* app, Window* win, bool force);
// Escape, a click elsewhere, or an edit that leaves nothing to complete.
void InputDismissCompletion(InputState* s);
// Accept the selected item: the query range is replaced by its insert text.
void InputAcceptCompletion(InputState* s, App* app, Window* win);
// The four keys the menu takes while it is up, answering whether it took the
// chord — `CompletionMenu::handle_action`.
bool InputCompletionAction(InputState* s, App* app, Window* win,
                           InputAction action);
// ShowCompletions: ctrl-space asks whatever the caret is on, which is Rust's
// second way in beside a trigger character.
void InputShowCompletions(InputState* s, App* app, Window* win);

// ─── document colours ─────────────────────────────────────────────────────

// Ask the provider what colours the document names, if it has changed since
// it was last asked. The row builder does this; a caller that publishes its
// own set writes `documentColors` and leaves the provider null.
void InputUpdateDocumentColors(InputState* s);

// ─── code actions ─────────────────────────────────────────────────────────

// ─── the overlay seam (input/editor/lsp/overlay.rs) ──────────────────────
//
// Rust keeps only the *state* of the two menus in the editor and hands the
// drawing to whoever is hosting it: an application can present its own items,
// draw its own popover, and take the keys the menu would have taken. This
// tree draws both menus itself — `component::Highlighter` does, under the
// caret — and everything below is the same seam beside it, so a host that
// wants its own can have one.

// present_completion_items: the host pushing a list in, rather than the
// editor pulling one from a provider. The items are the caller's and outlive
// the menu, the way a provider's do.
void InputPresentCompletionItems(InputState* s, int triggerStart, Str query,
                                 const CompletionItem* items, int n);
// present_code_actions, the same for the other menu.
void InputPresentCodeActions(InputState* s, const CodeActionItem* items, int n);
// present_hover / present_diagnostic / clear_diagnostic_popover: the two
// popovers, put up by the host rather than found by the editor.
void InputPresentHover(InputState* s, Selection symbolRange, Str text);
void InputPresentDiagnostic(InputState* s, int index);
void InputClearDiagnosticPopover(InputState* s);
// route_overlay_action: whichever menu is open takes the action. True when
// one did.
bool InputRouteOverlayAction(InputState* s, App* app, Window* win,
                             InputAction action);
// dismiss_completion_overlay / dismiss_code_action_overlay /
// dismiss_lsp_overlays.
void InputDismissLspOverlays(InputState* s);
// is_context_menu_open: either menu.
bool InputIsContextMenuOpen(const InputState* s);
// insert_completion: write one item in over `fallback`, which is what the
// menu's own accept does with the range the query occupied. A host that drew
// its own popover calls this with the item the reader picked.
void InputInsertCompletion(InputState* s, App* app, Window* win,
                           const CompletionItem* item, Selection fallback);

// The documentation of the item the selection is on, resolved through the
// provider the first time it is looked at. Empty when there is none, and
// what the item already carried when it came with some.
Str InputCompletionDocumentation(InputState* s);

// schedule_inline_completion: the typing stopped, so the provider may be
// asked once the debounce has run. Called by every edit, which is also what
// drops the suggestion that was showing.
void InputScheduleInlineCompletion(InputState* s);
// The frame's half of that clock: ask the provider if the debounce is up and
// nothing has moved. True when it wants another frame to keep waiting.
bool InputUpdateInlineCompletion(InputState* s, bool menuOpen);
// has_inline_completion / clear_inline_completion / accept_inline_completion.
bool InputHasInlineCompletion(const InputState* s);
void InputClearInlineCompletion(InputState* s);
bool InputAcceptInlineCompletion(InputState* s, App* app, Window* win);

// CodeActionProvider registration. The one-provider form is the field above;
// this is what a second and a third go through.
void InputAddCodeActionProvider(InputState* s, CodeActionFn fn, void* data,
                                CodeActionPerformFn perform = nullptr);

// Lsp::update: what the document changing under the LSP layer means — the
// caches that are derived from it are asked for again. Rust calls it from
// the edit path; the frame builder is where it is called here, since the
// answers are wanted for the frame being built.
void InputLspUpdate(InputState* s);
// Lsp::reset: everything the layer had cached or was showing, dropped.
void InputLspReset(InputState* s);

// Lsp::update's semantic half: ask the provider again when the document has
// changed under it. Nothing happens without a provider, which is every field
// that is not a code editor.
void InputUpdateSemanticTokens(InputState* s);

// handle_hover_definition: ask the definition provider about the offset the
// pointer is over, unless the last answer already covers it. What it finds is
// underlined in the editor and takes the hand cursor.
void InputHoverDefinition(InputState* s, int offset);
// The other half: the modifier came up, or the pointer left the field.
void InputClearHoverDefinition(InputState* s);
// handle_click_hover_definition: a secondary-click inside a symbol the hover
// found goes to its first location. True when it did, which is what keeps the
// same press from also moving the caret.
bool InputClickDefinition(InputState* s, App* app, Window* win, int offset,
                          bool secondary);
// The GoToDefinition action, which goes by the last thing a hover found
// rather than by what is under the pointer now — the pointer has moved on by
// the time a menu row is picked.
void InputGoToDefinition(InputState* s, App* app, Window* win);
// `can_go_to_definition`: whether the field has a provider at all, which is
// what greys the menu row out.
bool InputCanGoToDefinition(const InputState* s);
// go_to_definition: the host first, then the browser for an external uri, and
// otherwise the selection moved to the target inside this document.
void InputFollowDefinition(InputState* s, App* app, Window* win,
                           const DefinitionLink& link);

// ToggleCodeActions: ask the provider about what is selected and open the
// menu on what it offers. Nothing offered leaves the menu down.
void InputToggleCodeActions(InputState* s, App* app, Window* win);
void InputDismissCodeActions(InputState* s);
// Perform the selected action: its range is replaced by its text, as one
// undo step, and the menu goes away.
void InputPerformCodeAction(InputState* s, App* app, Window* win);
// The four keys the menu takes while it is up — `CodeActionMenu::
// handle_action`, which is the completion menu's, over the other list.
bool InputCodeActionAction(InputState* s, App* app, Window* win,
                           InputAction action);

// replace_text_in_range: the one path every edit goes through. A null range is
// the current selection. Returns false when the edit was rejected — readonly,
// or a mask or validator that said no.
bool InputReplaceTextInRange(InputState* s, App* app, Window* win,
                             const Selection* range, Str newText);
// Input methods count in UTF-16 on both platforms that have one to talk to;
// a field counts in bytes. These are the two directions across. An offset
// past the end clamps to it, which is what a platform handing over a stale
// range needs.
int Utf8OffsetToUtf16(Str s, int u8);
int Utf16OffsetToUtf8(Str s, int u16);
// marked_text_range(): what the input method is still deciding, if anything.
bool InputMarkedRange(const InputState* s, Selection* out);
// replace_and_mark_text_in_range(): the input method's provisional insert.
// A null `range` means "over the mark, or the selection if there is none",
// which is what makes each keystroke of a composition replace the last one.
// `sel` is where the caret should sit inside the new text, in bytes from its
// start; null puts it at the end. Empty text ends the composition.
void InputReplaceAndMarkText(InputState* s, App* app, Window* win,
                             const Selection* range, Str newText,
                             const Selection* sel);
// unmark_text(): the composition is over and what it left stands. Commits the
// undo transaction the composition opened, so the whole of it undoes at once.
void InputUnmarkText(InputState* s, App* app, Window* win);
// The typed character, once the platform has decoded it.
void InputTypeChar(InputState* s, App* app, Window* win, uint32_t ch);

// ─── the find bar, crates/base/src/input/editor/search.rs ─────────────────

// open_search: the bar opens over the field, with whatever is selected as
// its first query and the match nearest the top of the view as its first
// match. A field that is not searchable ignores it.
void InputOpenSearch(InputState* s, App* app, Window* win, bool replaceMode);
void InputCloseSearch(InputState* s, App* app, Window* win);
// is_replaceable(): the field allows it and is editable right now.
bool InputIsReplaceable(const InputState* s);
void InputSetSearchReplaceMode(InputState* s, App* app, Window* win, bool on);
void InputSetSearchQuery(InputState* s, App* app, Window* win, Str query,
                         bool insensitive);
// next_search_match / previous_search_match: the cursor moves, wrapping, and
// the view follows. False when nothing matched.
bool InputSearchNext(InputState* s, App* app, Window* win, Selection* out);
bool InputSearchPrev(InputState* s, App* app, Window* win, Selection* out);
// replace_current_search_match: the match under the cursor becomes the
// replacement, and the cursor is left on what is now under it.
bool InputSearchReplaceOne(InputState* s, App* app, Window* win, Str with);
// replace_all_search_matches: every match, back to front so the earlier
// offsets stay good. Answers how many there were.
int InputSearchReplaceAll(InputState* s, App* app, Window* win, Str with);
// update_search: the matcher takes the text as it is now. Every edit goes
// through this, so a find bar left open follows what is typed.
void InputUpdateSearch(InputState* s);

// on_focus / on_blur. Points win->input at this field, starts its caret, and
// emits the event; blurring commits the typing session, which is what makes a
// later undo stop at the right place.
void InputFocus(InputState* s, App* app, Window* win);
void InputBlur(InputState* s, App* app, Window* win);

// index_for_mouse_position: the offset a press at (x, y) lands on, against the
// run the element last painted.
int InputIndexForPosition(const InputState* s, PaintCtx* ctx, float x, float y);
// The fold chevron a press at (x, y) landed on, or -1. The boxes are the ones
// the last frame's gutter left behind.
int InputFoldIconAt(const InputState* s, float x, float y);
// Open or close the fold that starts on `line`, and redraw.
void InputToggleFold(InputState* s, App* app, Window* win, int line);
// apply_highlighter_fold_candidates: what the highlighter found, taken only
// when this field is a code editor with folding on.
void InputSetFoldCandidates(InputState* s, const FoldRange* ranges, int n);
// The field a press at (x, y) landed in, or null.
InputState* InputAtPosition(PaintCtx* ctx, float x, float y);

// crates/base/src/slider.rs. SliderValue is Rust's enum — `Single(f32)` or
// `Range(f32, f32)` — flattened into the pair plus the flag that says which
// variant this is. `hi` is `end()`, the one a single-value slider uses.
struct SliderValue {
    float lo = 0;
    float hi = 0;
    bool range = false;

    float Start() const { return range ? lo : hi; }
    float End() const { return hi; }
};

inline SliderValue SliderSingle(float v) {
    return {0, v, false};
}
inline SliderValue SliderRange(float lo, float hi) {
    return {lo, hi, true};
}
// SliderValue::clamp.
SliderValue SliderValueClamp(SliderValue v, float min, float max);
// SliderValue::set_start / set_end: a range keeps its ends in order.
void SliderValueSetStart(SliderValue* v, float value);
void SliderValueSetEnd(SliderValue* v, float value);

// SliderScale. Logarithmic gives finer control near the low end, which is what
// a volume or a playback speed wants.
enum class SliderScale : uint8_t {
    Linear,
    Logarithmic
};

// SliderEvent. Change comes on every press and every move that shifts the
// value; Release comes once, when the button goes up after one of those.
enum class SliderEventKind : uint8_t {
    Change,
    Release
};

struct SliderEvent {
    SliderEventKind kind = SliderEventKind::Change;
    SliderValue value = {};
};

// SliderState: what a slider is between frames, the way InputState is what an
// input is. Rust keeps it in an `Entity<SliderState>` and the element closures
// capture that handle; an element here names its state with `El::BindSlider`
// and the window applies the same behavior, which is how InputState works.
// `onChange` is `cx.subscribe(&state, |ev: &SliderEvent| ...)`.
struct SliderState {
    float min = 0;
    float max = 100;
    float step = 1;
    SliderValue value = {};
    // percentage: Range<f32>. A single-value slider only uses `hi`, with `lo`
    // pinned at 0, which is what Rust's `0.0..percentage` says.
    float pctLo = 0;
    float pctHi = 0;
    // The box the value maps against, recorded when the slider is pressed.
    Bounds bounds = {};
    SliderScale scale = SliderScale::Linear;
    // Set by a press, cleared by the release, so a release with no press
    // behind it emits nothing.
    bool dragging = false;
    // Which end of a range the press took, for the moves that follow. Rust
    // carries it in the DragThumb payload, which a drag here has no room for.
    bool dragStart = false;
    Listener onChange = {};
};

// SliderState::new().min(..).max(..).step(..).scale(..).default_value(..),
// which is a builder chain in Rust and one call here, so a view can write its
// slider as a field initializer.
SliderState SliderStateNew(float min, float max, SliderValue value,
                           float step = 1,
                           SliderScale scale = SliderScale::Linear);

// `.min()` / `.max()`, which re-derive the thumb position. Rust panics when a
// logarithmic slider is given a min <= 0 or a max <= min; there are no
// exceptions here, so the limits are pushed to the nearest usable pair
// instead — a widget that draws itself wrong is better than one that exits.
void SliderSetLimits(SliderState* s, float min, float max);
// `.step()`, the quantum a value snaps to.
void SliderSetStep(SliderState* s, float step);
// `.scale()`.
void SliderSetScale(SliderState* s, SliderScale scale);
// `.default_value()` / `set_value()`.
void SliderSetValue(SliderState* s, SliderValue v);
// set_bounds, the box a position maps against.
inline void SliderSetBounds(SliderState* s, Bounds b) {
    s->bounds = b;
}

// percentage_to_value / value_to_percentage.
float SliderPctToValue(const SliderState* s, float pct);
float SliderValueToPct(const SliderState* s, float value);
// update_thumb_pos: the percentages that follow from the value.
void SliderUpdateThumbPos(SliderState* s);

// update_value_by_position. `isStart` moves the low end of a range. Rust ends
// this with `cx.emit(SliderEvent::Change)`; the window here raises the event
// instead, so this returns whether the value actually moved.
bool SliderUpdateByPosition(SliderState* s, Axis axis, Point pos, bool isStart);
// Which end of a range a press at `pos` takes, by the midpoint between the two
// thumbs — the `is_start` arm of SliderTrack's mouse-down handler.
bool SliderIsStartAt(const SliderState* s, Axis axis, Point pos);
// handle_release: true when a Release event is due.
bool SliderHandleRelease(SliderState* s);
// The a11y half of slider.rs that is reachable here: `on_a11y_action` binds
// Increment and Decrement, and this is what the two of them do to the value.
// There is no accessibility layer in this tree, so the arrows are what carries
// them — and the arrows are what a keyboard user needs either way. `dir` is +1
// or -1, and `isStart` picks which end of a range moves. Answers whether the
// value moved.
bool SliderStepBy(SliderState* s, int dir, bool isStart);

struct Overlay {
    int kind = 0; // 0 none, 1 dialog, 2 sheet
    char title[128] = {};
    char body[2048] = {};
};

struct MenuState {
    bool open = false;
    float x = 0, y = 0;
    char items[8][32] = {};
    int nItems = 0;
    int clickBase = 0;
};

struct WinSize {
    float dipW = 0;
    float dipH = 0;
    int pxW = 0;
    int pxH = 0;
};

float PxToDip(PaintCtx* ctx, int px);
int DipToPx(PaintCtx* ctx, float dip);

Size MeasureText(PaintCtx* ctx, Str s, float fontSize, float maxW,
                 bool wrap = false, int weight = 0, float lineH = 0);
// One run drawn from its baseline rather than from the top of its line box,
// which is the point an SVG <text> names. Its one caller is drawops.cpp, and
// it lives here rather than there because the run has to go through the
// frame's measurement cache: the cache is what holds the shaped run after the
// walk that drew it returns, and the scene replays a text primitive by the
// pointer it recorded.
void DrawTextBaseline(PaintCtx* ctx, Str s, float x, float baselineY,
                      float fontSize, Rgba color, int weight = 0);
void TextMeasBeginFrame(PaintCtx* ctx);
void TextMeasEndFrame(PaintCtx* ctx);
void TextMeasClear(PaintCtx* ctx);
// The inverse of TextIndexAt: where a UTF-8 offset sits in a run once it has
// been laid out and wrapped. `outY` is the top of the visual row the offset
// is on and `outH` that row's height. False when there was nothing to
// measure against.
//
// `mono` and `lineHeight` have to be the ones the run was painted with:
// Consolas advances differently from the proportional face, so measuring a
// code editor's row without them answers for a line of text that was never
// drawn.
bool TextPointAt(PaintCtx* ctx, Str s, float fontSize, float maxW, bool wrap,
                 int off, float* outX, float* outY, float* outH,
                 bool mono = false, float lineHeight = 0);
int TextIndexAt(PaintCtx* ctx, Str s, float fontSize, float maxW, bool wrap,
                float relX, float relY, bool mono = false,
                float lineHeight = 0);
// `weight` and `lineH` have to be the ones the run was laid out with, or the
// rects come back measured against a different font: the mono family is a
// weight sentinel here, so a code row measured with 0 drifts further from the
// glyphs the further along the line it is.
void PaintTextRange(PaintCtx* ctx, Str s, float fontSize, float maxW, bool wrap,
                    uint8_t weight, float lineH, float x, float y, int u8a,
                    int u8b, Rgba color);
void PaintTextUnderline(PaintCtx* ctx, Str s, float fontSize, float maxW,
                        bool wrap, float x, float y, int u8a, int u8b,
                        Rgba color, bool wavy = false);
// The taffy tree a window lays out in, kept between frames so taffy's own
// per-node caches are. It is reconciled against the element tree rather than
// rebuilt: an element whose style and content are the ones its node already
// has is not laid out again, and a subtree of them is not walked at all.
// See the block above `LayoutNode` in gpui.cpp, and GPUI_LAYOUT_REUSE=off to
// take it back out.
struct LayoutCache;

LayoutCache* LayoutCacheNew();
void LayoutCacheFree(LayoutCache* lc);

// What the last frame's reconcile did, for GPUI_FRAME_BENCH.
struct LayoutCacheStats {
    // Nodes the element tree asked for.
    int nodes = 0;
    // Of those, the ones that had to be made — a page that has just changed
    // makes them all, a page that has not makes none.
    int made = 0;
    int dropped = 0;
    // Nodes told about a new style, and measured leaves told their content
    // moved. Each of those is a node taffy has to lay out again, and its
    // ancestors with it.
    int restyled = 0;
    int remeasured = 0;
};

LayoutCacheStats LayoutCacheLastStats(const LayoutCache* lc);
// The scratch cache MeasureEl and a cache-less LayoutEl share, given back at
// AppFree.
void LayoutScratchFree();

// `lc` is the window's cache, and the reason layout is cheap on a frame that
// changed little. A caller without one — a test, a measurement — passes none
// and gets a scratch cache that is reset per call, which is what every caller
// got before there was a cache at all.
void LayoutEl(PaintCtx* ctx, El* e, float x, float y, float availW,
              float availH, float inheritFont, Rgba inheritFg,
              LayoutCache* lc = nullptr);
// AnyElement::layout_as_root(size(MinContent, MinContent)): what one element
// wants to be, laid out on its own and away from the tree it will go into.
// A virtualized list measures a row this way and then places every row at
// that size, since it cannot ask the layout what the rows it did not build
// would have come out as.
//
// It runs the same pass `LayoutEl` does and leaves the boxes on the element,
// so the caller may either read the returned size or go on to use `e`. It has
// a layout cache of its own, reset per call, so a measure in the middle of a
// frame does not disturb the window's.
Size MeasureEl(PaintCtx* ctx, El* e, float inheritFont = 0,
               Rgba inheritFg = {});
void PaintEl(PaintCtx* ctx, El* e);
int HitTest(PaintCtx* ctx, float x, float y);
const HitRect* HitTestRect(PaintCtx* ctx, float x, float y);
// The topmost element under the pointer that takes a drag of this kind, of
// those that asked for one at all. A drop target that does not want what is
// being dragged is not in the way of one that does.
const HitRect* HitTestDrop(PaintCtx* ctx, float x, float y, Str kind);
const ScrollRect* HitScrollRect(PaintCtx* ctx, float x, float y);
int TextHitOffsetAt(PaintCtx* ctx, float x, float y, bool nearest);
// The same, confined to one selection scope (-1 for any), and reporting the
// scope the point landed in.
int TextHitOffsetIn(PaintCtx* ctx, float x, float y, bool nearest, int scope,
                    int* outScope);
int CopyTextHits(PaintCtx* ctx, int selA, int selB, char* out, int cap);
// `fmt` is what each run contributes: its rendered text, or — where the run
// carries a SelSource — the Markdown it was rendered from.
int CopyTextHitsIn(PaintCtx* ctx, int selA, int selB, int scope, char* out,
                   int cap, SelectionFormat fmt = SelectionFormat::Plain);
// points_for_multi_click: the document range a press of `clickCount` selects
// under (x, y) — 2 takes the word, 3 or more the whole run — in the same
// offsets TextHitOffsetAt and CopyTextHits speak. False for a single click,
// or when no selectable text is there.
bool TextMultiClickRange(PaintCtx* ctx, float x, float y, int clickCount,
                         int* outA, int* outB);
bool TextMultiClickRangeIn(PaintCtx* ctx, float x, float y, int clickCount,
                           int scope, int* outA, int* outB, int* outScope);
int HashClickId(Str s);

// Which of the three fills a box paints. GPUI resolves this by refining the
// hovered style and then the active one over the base, so the pressed fill
// wins where a box has both and is being held. Split out from the paint pass
// because it is the whole of what `Style::activeBg` means and the pointer
// cannot be driven from a test.
//
// Both states need a click id of their own: without one the box would match a
// `hoverId` / `activeId` of 0, which is what "nothing is hovered" and
// "nothing is held" are spelled as.
enum class BoxFill : uint8_t {
    Base,
    Hover,
    Active
};
BoxFill BoxFillFor(bool hasActiveBg, bool hasHoverBg, int clickId, int activeId,
                   int hoverId);

// Whether a release makes a click. GPUI holds the press as
// `pending_mouse_down` and fires on_click from the mouse-up handler, where it
// asks three things: that a press is waiting at all, that the button coming
// up is the one that went down, and that the element under the pointer is the
// one the press landed on. A press that slid off somewhere else is no click,
// and a drag takes the release the click would have had.
//
// `pending` is Rust's Option being Some: a press the scrollbar, the inspector
// or a non-focusing button took is nobody's pending click. `upId` and
// `pressedId` are 0 for the page itself, which is a click too — that is the
// outside press an overlay dismisses on.
bool ClickFromRelease(bool pending, int pressedId, MouseButton pressedButton,
                      bool dragged, int upId, MouseButton upButton);

// The same for the keyboard: whether the release of Enter or Space makes a
// click. GPUI arms on the key down — `pending_keyboard_down` is the focus
// generation the keystroke went down at — and makes the click from the key
// up, if that stamp still matches. A generation rather than the focused
// element itself, because focus that left and came back is not the same
// press; any other key in between clears the stamp, and a modifier held on
// either half means the keystroke was a shortcut, not an activation.
bool ClickFromKeyRelease(bool pending, int pendingGen, int focusGen, int key,
                         bool modified);

// The window chrome's own click ids (WM_NCHITTEST). Negative, so they cannot
// collide with a hashed one: HashClickId is non-negative by construction.
// They were 100/101/102/200 once, and collided with the showcase overview
// grid — which is what a flat id space costs when two people pick numbers.
enum {
    ClickWinMin = -1,
    ClickWinMax = -2,
    ClickWinClose = -3,
    ClickWinCaption = -4,
};

struct App;
struct Window;

struct WinOpts {
    bool borderless = false;
    // The view draws the title bar. Cocoa keeps its traffic-light controls
    // above a transparent full-size content view; Windows drops the caption
    // but keeps the rest of the frame, and X11 drops the frame outright. On
    // all three the view supplies the title-bar background, its drag region
    // and — off macOS — the minimize / maximize / close controls, which is
    // what component::TitleBar builds.
    bool clientTitleBar = false;
    bool anim = false;
    int timerMs = 500;
};

// gpui::FrameTiming. One drawn frame, as measured by the window itself, so the
// FPS HUD reports what the runtime actually spent rather than an approximation
// taken from the outside. GPUI gates recording behind
// `set_frame_trace_enabled`; here it is two QPC reads per frame and always on.
struct FrameTiming {
    float drawSecs = 0;
};

enum {
    kFrameTraceCap = 256
};

// Process-wide state: the Direct2D / DirectWrite factories, the shared font
// cache, the entity store and the open windows. GPUI's `App`.
// One live cx.subscribe. GPUI keys its subscriber lists by the emitter and
// hands back a Subscription that unsubscribes when it drops; a handle here is
// an id, and a subscription whose emitter or subscriber has gone stale is
// swept the next time the emitter emits.
struct EntitySub {
    int id = 0;
    EntityId emitter = {};
    Listener handler = {};
};

struct App {
    PaintApp* paint = nullptr;
    ThemeMode themeMode = ThemeMode::Light;
    Vec<Window*> windows;
    // Entity store; see Entity.h. Slots are recycled, so a handle carries a
    // generation and goes stale instead of dangling.
    Vec<EntitySlot> entities;
    Vec<int32_t> freeSlots;
    // cx.subscribe: every live subscription, in the order they were made.
    Vec<EntitySub> subs;
    // cx.observe: the same for the untyped channel, where `emitter` is the
    // entity being watched.
    Vec<EntitySub> observers;
    int nextSubId = 1;
    int exitCode = 0;

    App() = default;
};

// One platform window: its render target, frame arena, hover / focus state
// and the view it renders. GPUI's `Window`.
// The OS window: an HWND wrapper on Windows, an X11 Window on Linux.
struct PlatWindow;

// crates/base/src/text_selection.rs WindowSelectionState, one per window.
struct WindowSelection;

struct Window {
    App* app = nullptr;
    PlatWindow* plat = nullptr;
    PaintCtx paint = {};
    Arena* frameArena = nullptr;
    // The taffy tree this window lays out in, kept between frames. Made on
    // the first frame and freed with the window.
    LayoutCache* layout = nullptr;
    // The selection over this window's selectable runs. Made on the first
    // press that lands on text and dropped with the window.
    WindowSelection* sel = nullptr;
    // The view this window renders. GPUI's Window holds a root view too.
    EntityId root = {};
    // Every entity `EntityRender` was asked for while the last frame was
    // built — GPUI's `Window::dirty_views`, and what makes `Notify` name a
    // window rather than every window. Rebuilt each frame.
    Vec<EntityId> rendered;
    int hoverId = 0;
    int focusId = 0;
    // window.focus_generation: bumped every time the focus moves, so a
    // keystroke can tell that it stayed put without holding onto the element.
    int focusGen = 0;
    float mouseX = 0;
    float mouseY = 0;
    // The modifiers the pointer last moved or pressed under. Rust reads them
    // off the MouseMoveEvent inside its handler; a hover here is worked out
    // by the frame builder, which has no event to read, so the window keeps
    // the last ones. What wants them: a secondary-hover over a symbol.
    Modifiers mouseModifiers = {};
    // What the pointer looks like right now; the OS is only told on a change.
    CursorKind cursor = CursorKind::Arrow;
    bool maximized = false;
    // is_window_active: whether this window has the focus. A client-decorated
    // frame dims its border when it does not.
    bool active = true;
    bool running = true;
    bool anim = false;
    // window.request_animation_frame(): one more frame after this one, asked
    // for while the frame is being built and cleared as the next one starts,
    // so something that has finished moving stops asking. `anim` is the other
    // thing — a window that draws back to back until it is turned off.
    bool animFrame = false;
    // The instant this frame started. Every transition in it samples the same
    // `now`, which is what Rust gets from reading the executor's clock.
    double frameNow = 0;
    bool mouseDown = false;
    // cx.stop_propagation(): set by a handler, read by the chain it is in.
    bool stopPropagation = false;
    // The multi-click run in progress: when the last press landed, where, and
    // with which button, so WindowClickCount can tell the next press apart
    // from a second click. GPUI keeps the same three in its platform layer.
    double lastDownAt = 0;
    float lastDownX = 0;
    float lastDownY = 0;
    MouseButton lastDownButton = MouseButton::Left;
    int clickRun = 0;
    // The element that took the press, until the button comes back up: what
    // GPUI's drag gives an element for free. 0 when nothing is held.
    int pressedId = 0;
    // What the press that is still down looked like: GPUI keeps the whole
    // MouseDownEvent as `pending_mouse_down` and hands it to the click it
    // makes on release, which is where the count and the modifiers come from.
    int pressedCount = 1;
    // pending_mouse_down being Some: a press is waiting to become a click.
    bool pressPending = false;
    // Where the press landed, and whether the pointer has since travelled far
    // enough to call it a drag. GPUI starts a drag from the move rather than
    // the press, and a drag takes the release the click would have had.
    float pressedX = 0;
    float pressedY = 0;
    bool pressedMoved = false;
    MouseButton pressedButton = MouseButton::Left;
    Modifiers pressedModifiers = {};
    // The drag in flight: what the press picked up, and which drop target the
    // pointer is over right now. GPUI keeps the same pair — `active_drag` and
    // the hitbox its drop handlers consult — on its Window.
    DragPayload activeDrag = {};
    int dragOverId = 0;
    // AnyDrag::cursor_offset: where inside the dragged element the press
    // landed, so whatever is drawn under the pointer sits where the element
    // was rather than jumping its corner to the cursor.
    float dragOffX = 0;
    float dragOffY = 0;
    bool eatReturn = false;
    // The keystroke the keymap took also arrives as a character, and a
    // character the keymap took is not text. Set on the key, read and cleared
    // on the character that follows it.
    bool eatChar = false;
    // pending_keyboard_down: an Enter or Space is down on the focused
    // element, and the generation the focus was at when it went down.
    bool keyPressPending = false;
    int keyPressGen = 0;
    // The scrollbar being dragged, and how far into its thumb the press
    // landed. GPUI keeps the same pair in ScrollbarState::drag_pos.
    int scrollDragId = 0;
    float scrollDragGrab = 0;
    // Which of the box's two bars is being dragged.
    bool scrollDragHorizontal = false;
    InputState* input = nullptr;
    // This window's one TooltipOverlay. Created on first use, the way a
    // field's blink cursor is.
    EntityId tooltip = {};
    Overlay overlay = {};
    InspectorState inspector = {};
    MenuState menu = {};
    Vec<FocusRect> focusEls;
    // The focus trap a container asked to hold focus this frame, settled
    // after the focusables are collected. 0 when nothing asked.
    int pendingTrap = 0;
    // The trap container's own focus id, for a trap that holds no tab stop.
    int pendingTrapHost = 0;
    Vec<KeyedSlot> keyed;
    // The frame's key contexts and action handlers, in tree order.
    Vec<DispatchNode> dispatch;
    Vec<MotionSlotRec> motionSlots;
    WinOpts opts = {};
    // Window-level subscriptions bound to view entities.
    Listener onKey = {};
    Listener onClick = {};
    Listener onMouseDown = {};
    Listener onMouseUp = {};
    Listener onMouseMove = {};
    Listener onMouseExit = {};
    Listener onScrollWheel = {};
    // Armed timers, any number of them.
    Vec<TimerSub> timers;
    int nextTimerId = 1;
    // Which InputState had focus last frame, so the runtime can start and stop
    // its caret without every app wiring that up.
    InputState* prevInput = nullptr;
    // Ring of the last kFrameTraceCap draw times; frameSeq counts every frame
    // ever drawn and is what a collector cursors on.
    FrameTiming frameTrace[kFrameTraceCap] = {};
    uint64_t frameSeq = 0;

    Window() = default;
    // Drops the focus registration a field is still holding on this window.
    // The mirror of ~InputState, which drops the one this window is holding
    // on the field; see window_common.cpp.
    ~Window();
};

// ─── context ──────────────────────────────────────────────────────────────

// GPUI's Context<T>. `win` is null outside a window callback, `a` is the frame
// arena during render, `self` is the entity currently rendering or updating.
struct Ctx {
    App* app = nullptr;
    Window* win = nullptr;
    Arena* a = nullptr;
    EntityId self = {};
    // The ids of the widgets this one is being built inside, folded together
    // — window.element_id_stack, which GPUI has already pushed by the time a
    // child's render runs. The port builds its tree before IdsCollect folds
    // anything, so a widget that names itself pushes it here as well, and a
    // `use_keyed_state` asked for underneath is keyed by the whole stack.
    // See IdScope.
    uint32_t path = 0;

    // cx.theme() — the colors every widget reads.
    const Theme& theme() const;
    ThemeMode themeMode() const;
};

// The same fold IdsCollect uses on the element tree, over one more name.
uint32_t IdFoldName(uint32_t parent, Str name);

// with_element_id: the widget's name is on the stack while its parts are
// built, and off it again afterwards. A widget that owns a name pushes one of
// these at the top of the function that builds its children.
struct IdScope {
    Ctx* cx = nullptr;
    uint32_t prev = 0;

    IdScope(Ctx* c, Str name) : cx(c), prev(c ? c->path : 0) {
        if (cx) {
            cx->path = IdFoldName(prev, name);
        }
    }
    ~IdScope() {
        if (cx) {
            cx->path = prev;
        }
    }
    IdScope(const IdScope&) = delete;
    IdScope& operator=(const IdScope&) = delete;
};

EntityId EntityNewRaw(App* app, void* ptr, RenderFn render, DropFn drop);
void* EntityGet(App* app, EntityId id);
void EntityDrop(App* app, EntityId id);
void EntityDropAll(App* app);

// A typed handle. Stale handles read back as null instead of dangling.
template <typename T>
struct Entity {
    EntityId id = {};

    bool IsValid() const { return id.IsValid(); }
    T* Get(App* app) const { return (T*)EntityGet(app, id); }
    T* Get(Ctx* cx) const { return (T*)EntityGet(cx->app, id); }
};

template <typename T>
void EntityDropT(void* p) {
    delete (T*)p;
}

// cx.new(|cx| T::default()). T must expose `static El* Render(T*, Ctx*)`.
template <typename T>
Entity<T> EntityNew(App* app) {
    Entity<T> e;
    e.id = EntityNewRaw(app, new T(), (RenderFn)&T::Render, &EntityDropT<T>);
    return e;
}

template <typename T>
Entity<T> EntityNew(Ctx* cx) {
    return EntityNew<T>(cx->app);
}

// State with no Render, e.g. a model the views read.
template <typename T>
Entity<T> EntityNewState(App* app) {
    Entity<T> e;
    e.id = EntityNewRaw(app, new T(), nullptr, &EntityDropT<T>);
    return e;
}

// cx.listener(|this, ev, window, cx| ...). The cast mirrors MkFunc0/MkFunc1.
// E is whichever event struct the handler takes: ClickEvent, KeyEvent, ...
template <typename T, typename E>
Listener Listen(Ctx* cx, void (*fn)(T*, Ctx*, const E*)) {
    Listener l;
    l.fn = (void*)fn;
    l.view = cx->self;
    return l;
}

// cx.listener(move |this, ...| ... ix ...): same, carrying a captured value.
template <typename T, typename E>
Listener Listen(Ctx* cx, void (*fn)(T*, Ctx*, const E*, intptr_t),
                intptr_t arg) {
    Listener l;
    l.fn = (void*)fn;
    l.view = cx->self;
    l.arg = arg;
    l.hasArg = true;
    l.argBound = true;
    return l;
}

// A handler that takes a value the component supplies: which day of the
// calendar, which combobox row. The component fills it with ListenerArg.
template <typename T, typename E>
Listener Listen(Ctx* cx, void (*fn)(T*, Ctx*, const E*, intptr_t)) {
    Listener l;
    l.fn = (void*)fn;
    l.view = cx->self;
    l.hasArg = true;
    return l;
}

// Bind the value a component hands its caller. This is what a Rust closure
// gets as its event payload: `.on_click(cx.listener(|this, day, _, cx| ...))`.
inline Listener ListenerArg(Listener l, intptr_t arg) {
    if (l.IsValid()) {
        l.arg = arg;
        l.hasArg = true;
        l.argBound = true;
    }
    return l;
}

// The same, for the value a widget produces rather than one its caller chose:
// which day of the calendar, the state a checkbox activation lands on. Rust
// passes that beside whatever the closure captured, so a caller that already
// bound its own — which of ten toggles this is — keeps it.
inline Listener ListenerFill(Listener l, intptr_t v) {
    if (l.IsValid() && !l.argBound) {
        l.arg = v;
        l.hasArg = true;
    }
    return l;
}

// Same, but bound to another entity instead of the one that is rendering.
template <typename T, typename E>
Listener ListenTo(Entity<T> e, void (*fn)(T*, Ctx*, const E*)) {
    Listener l;
    l.fn = (void*)fn;
    l.view = e.id;
    return l;
}

// The same fill-me-in handler as the third Listen above, bound to another
// entity: the component supplies the value — which menu row was taken —
// rather than the caller having captured one.
template <typename T, typename E>
Listener ListenTo(Entity<T> e, void (*fn)(T*, Ctx*, const E*, intptr_t)) {
    Listener l;
    l.fn = (void*)fn;
    l.view = e.id;
    l.hasArg = true;
    return l;
}

template <typename T, typename E>
Listener ListenTo(Entity<T> e, void (*fn)(T*, Ctx*, const E*, intptr_t),
                  intptr_t arg) {
    Listener l;
    l.fn = (void*)fn;
    l.view = e.id;
    l.arg = arg;
    l.hasArg = true;
    l.argBound = true;
    return l;
}

// ─── EventEmitter (crates/gpui/src/app/entity_map.rs, subscriber lists) ───
//
// Rust marks what an entity emits with `impl EventEmitter<E> for T`, sends one
// with `cx.emit(e)`, and hears it with `cx.subscribe(&entity, ..)`, which
// hands back a Subscription that unsubscribes when it drops. There is no trait
// to mark here: an emitter is an entity, an event is whatever struct the
// emitter sends, and the subscriber's handler has to take that same type —
// which is the one thing Rust checks and this cannot.
//
// A Subscription is a handle rather than a guard: nothing is destroyed on
// scope exit in a tree of POD state, so it is dropped by asking. It rarely
// has to be: a subscription whose subscriber has gone away is swept on the
// next emit, which is the lifetime that mattered.
struct Subscription {
    int id = 0;

    bool IsValid() const { return id != 0; }
};

Subscription EntitySubscribeRaw(App* app, EntityId emitter, Listener handler);
void EntityUnsubscribe(App* app, Subscription sub);

// ─── observers (crates/gpui/src/app.rs, `observers`) ─────────────────────
//
// `cx.observe(&entity, |this, observed, cx| ..)`: the untyped half of the
// pair above. An emitter sends an event and says what it is; an entity that
// notifies says only that it changed, and whoever is watching hears about it.
// The handler is called with the entity that notified, so one observer can
// watch several.
Subscription EntityObserveRaw(App* app, EntityId observed, Listener handler);
void EntityUnobserve(App* app, Subscription sub);
int EntityObserverCount(App* app, EntityId observed);

// `cx.notify()` for an entity that is not the one in hand, and the whole of
// what `Notify(cx)` does: run the observers, then invalidate the windows that
// rendered that entity last frame. A window that has never rendered it — a
// state entity that is not a view, a view whose first frame has not been
// built — falls back to `from`, and to every window when there is none, which
// is what this did for everything before there was anything to be precise
// with.
void NotifyEntity(App* app, EntityId id, Window* from);
// cx.emit(ev): every live subscriber hears it, oldest first. `ev` is the
// event struct the emitter sends, by pointer, and it does not outlive the
// call.
void EntityEmit(App* app, Window* win, EntityId emitter, const void* ev);
// How many subscriptions the emitter has, stale ones swept first. For a
// caller that has to know whether anybody is listening at all.
int EntitySubscriberCount(App* app, EntityId emitter);

// cx.subscribe(&emitter, cx.listener(..)): the handler belongs to whatever is
// rendering, the way Listen's does, and runs whenever `emitter` emits an E.
template <typename T, typename S, typename E>
Subscription Subscribe(Ctx* cx, Entity<T> emitter,
                       void (*fn)(S*, Ctx*, const E*)) {
    Listener l;
    l.fn = (void*)fn;
    l.view = cx->self;
    return EntitySubscribeRaw(cx->app, emitter.id, l);
}

// cx.observe(&entity, ..): `handler` runs on `cx->self` whenever `observed`
// notifies. The event pointer a listener takes is the notifying EntityId,
// since an observer that watches more than one has to tell them apart.
template <typename T, typename S>
Subscription Observe(Ctx* cx, Entity<T> observed,
                     void (*handler)(S*, Ctx*, const EntityId*)) {
    Listener l = Listen(cx, handler);
    return EntityObserveRaw(cx->app, observed.id, l);
}

// The same for a caller that has the observer's handle rather than a Ctx.
template <typename T, typename S>
Subscription ObserveTo(App* app, Entity<T> observed, Entity<S> observer,
                       void (*handler)(S*, Ctx*, const EntityId*)) {
    Listener l = ListenTo(observer, handler);
    return EntityObserveRaw(app, observed.id, l);
}

// The same, for a subscriber that is not the one rendering.
template <typename T, typename S, typename E>
Subscription SubscribeTo(App* app, Entity<T> emitter, Entity<S> subscriber,
                         void (*fn)(S*, Ctx*, const E*)) {
    Listener l;
    l.fn = (void*)fn;
    l.view = subscriber.id;
    return EntitySubscribeRaw(app, emitter.id, l);
}

// cx.emit(ev) from inside the emitter: `emitter` is the entity sending it,
// which a state that does not know its own handle takes as an argument.
template <typename E>
void Emit(Ctx* cx, EntityId emitter, const E* ev) {
    EntityEmit(cx->app, cx->win, emitter, ev);
}

// cx.notify(): the frame tree is rebuilt from scratch, so this just schedules
// a repaint of every window. GPUI tracks which views observe the entity.
void Notify(Ctx* cx);
void NotifyApp(App* app);
void ListenerCall(App* app, Window* win, const Listener& l, const void* ev);

// Render an entity into `a`, building the Ctx for it.
El* EntityRender(App* app, Window* win, Arena* a, EntityId id);

// window.use_keyed_state(key, cx, init)
void* WindowKeyedState(Window* win, uint32_t key, int size, DropFn drop);
void WindowKeyedFree(Window* win);

// The transition state behind one id, created zeroed on first ask and marked
// as wanted by this frame. Everything about it is in base/motion.h; this is
// the store, which has to live with the window.
void* WindowMotionState(Window* win, uint32_t key, int size);
// Drop what this frame did not ask for, which is what GPUI does with the
// state of an element it no longer renders.
void WindowMotionSweep(Window* win);
void WindowMotionFree(Window* win);

template <typename T>
T* KeyedState(Ctx* cx, uint32_t key) {
    void* p = WindowKeyedState(cx->win, key, (int)sizeof(T), &EntityDropT<T>);
    return (T*)p;
}

// The same window-keyed state, as an entity. `use_keyed_state` in Rust hands
// back an `Entity<T>`, which is what lets the state own timers and listeners
// of its own; KeyedState above is only a pointer, so nothing can be bound to
// it. A widget whose behavior outlives one frame — a hover card counting down
// to open — needs this one.
EntityId WindowKeyedEntity(Window* win, App* app, uint32_t key, void* fresh,
                           DropFn drop);

// A keyed slot is remembered by its key alone, so two different states under
// one name are one slot — and the second would read the first's memory as its
// own. `kind` is a constant per state type (the hash of its name is what the
// callers pass), which is what keeps, say, a popover's own state and the
// listeners its escape runs apart.
inline uint32_t KeyedKey(uint32_t name, uint32_t kind) {
    uint32_t h = name * 2654435761u;
    h ^= kind + 0x9e3779b9u + (h << 6) + (h >> 2);
    return h ? h : 1u;
}

// use_keyed_state's key: the name folded into the stack of ids above it, so
// the same local name under two different widgets is two states. A caller
// that has already qualified its name by hand gets the same answer it did
// before, since nothing above it has pushed a scope.
inline uint32_t KeyedName(Ctx* cx, Str name) {
    return IdFoldName(cx ? cx->path : 0, name);
}

template <typename T>
Entity<T> KeyedEntity(Ctx* cx, uint32_t key) {
    Entity<T> e;
    e.id = WindowKeyedEntity(cx->win, cx->app, key, new T(), &EntityDropT<T>);
    return e;
}

// window.with_element_state(global_id, ..): state that belongs to one element
// rather than to the view that built it. The key is the element's name folded
// into the stack of ids above it — which is what a GlobalElementId is — so
// the same local name under two widgets is two states. `kind` is the state's
// own name, since a key remembers a slot and not what was put in it.
template <typename T>
T* ElementState(Ctx* cx, Str name, Str kind) {
    return KeyedState<T>(cx, KeyedKey(KeyedName(cx, name), HashClickId(kind)));
}

// The same, as an entity — for state a listener has to be bound to, which is
// what an element that answers its own presses needs.
template <typename T>
Entity<T> ElementStateEntity(Ctx* cx, Str name, Str kind) {
    return KeyedEntity<T>(cx, KeyedKey(KeyedName(cx, name), HashClickId(kind)));
}

// Window-level subscriptions. GPUI spells these window.on_key_down and
// cx.spawn + Timer::after; here each one is a Listener bound to a view.
void WindowOnKey(Window* win, Listener l);
// Fires for a click no element handled — the outside click that dismisses an
// overlay. Elements carry their own listener; this is not a dispatch table.
void WindowOnUnhandledClick(Window* win, Listener l);

// window.toggle_inspector, and the picking mode its magnifier button starts.
// Ctrl+Shift+I toggles it, as it does in Rust on everything but macOS.
void WindowToggleInspector(Window* win);
void WindowInspectorPick(Window* win, bool picking);
const InspectorState* WindowInspector(Ctx* cx);

// window.active_drag: what a press picked up, or an invalid payload when
// nothing is being dragged. A drop target reads it to decide whether to show
// itself at all, the way Rust's `drag_over::<T>` only exists for a drag of
// that type.
const DragPayload* WindowActiveDrag(Ctx* cx);
// window.is_window_active().
bool WindowIsActive(Ctx* cx);
// What the platform calls when the window takes or loses the focus.
void WindowSetActive(Window* win, bool active);
// Which element the drag is over, of those that take its kind — 0 for none.
// `El::Click(id)` names an element, so this answers with that id.
int WindowDragOverId(Ctx* cx);
// cx.stop_propagation(): the rest of the chain does not hear the event the
// handler is in. Only an element's mouse handler has a chain to stop.
void WindowStopPropagation(Ctx* cx);
// The same cursor offset, for whoever draws the thing being dragged.
Point WindowDragOffset(Ctx* cx);
// One subscription per event type, which is what window.on_mouse_event::<T>
// asks for in Rust. Each handler takes the matching event:
// `void OnDown(T* self, Ctx* cx, const MouseDownEvent* ev)`.
void WindowOnMouseDown(Window* win, Listener l);
void WindowOnMouseUp(Window* win, Listener l);
void WindowOnMouseMove(Window* win, Listener l);
void WindowOnMouseExit(Window* win, Listener l);
void WindowOnScrollWheel(Window* win, Listener l);
// cx.spawn(async move |this, cx| ...) with nothing to await: run `l` against
// its entity on the main thread, once this pass of the event loop is over.
// Safe to call from a worker thread, which is the point — a background job
// finishes with one of these rather than touching an entity itself.
//
// `ev` is what the listener receives, and must outlive the post; it is
// usually the job struct the worker filled in, and freeing it is the last
// thing the listener does. The post is dropped if the window has closed or
// the entity has gone by the time it runs, which is what Rust's
// `this.update(cx, ..).ok()` swallows.
//
// See sys/executor.h for the half of this that has no window: ExecPost.
void WindowPost(Window* win, Listener l, const void* ev = nullptr);

// Repeating timer; GPUI's system_monitor does the same with a spawned task
// that sleeps and calls cx.notify(). Returns a handle, or 0. Any number may
// be armed at once.
int WindowSetInterval(Window* win, int ms, Listener l);
// Fires once, then forgets itself. GPUI's Timer::after.
int WindowSetTimeout(Window* win, int ms, Listener l);
void WindowCancelTimer(Window* win, int id);

// ─── caret ────────────────────────────────────────────────────────────────
//
// Port of crates/base/src/input/base/blink_cursor.rs. A blinking caret is
// state, not a function of the clock: something flips it on a 500 ms timer
// and every repaint in between shows what the last flip decided. Sampling the
// clock at paint time instead makes the caret invisible whenever nothing
// happens to repaint during the lit half.
//
// One per text field, the way Rust gives every InputState its own
// Entity<BlinkCursor>. `handle` is an EntityId the owner keeps; the first
// Start creates the entity behind it.

struct BlinkCursor {
    bool visible = false;
    bool paused = false; // solid, because the user is typing
    // The armed timer. Cancelling it is what Rust's epoch counter does.
    int timer = 0;

    static void OnFlip(BlinkCursor* self, Ctx* cx, const TickEvent* ev);
    static void OnResume(BlinkCursor* self, Ctx* cx, const TickEvent* ev);
};

// Port of TooltipOverlay in crates/base/src/tooltip.rs. One per window, the
// way Rust keeps one overlay for every trigger in it, and an entity because it
// owns timers — the same reason BlinkCursor below is one.
//
// A tooltip is not "the hovered element has text". It waits out SHOW_DELAY
// before appearing, and on the way out it keeps a GRACE_PERIOD during which
// `hadRecent` is set: a trigger entered inside that window shows at once
// rather than making the reader wait again for a tip they were already
// reading. Rust guards each countdown with an epoch; there is at most one in
// flight here, so it is cancelled outright instead.
struct TooltipOverlay {
    // Heap-owned: the text came off a frame arena that is gone by the time the
    // countdown lands.
    Str text = {};
    Bounds triggerBounds = {};
    bool visible = false;
    bool hadRecent = false;
    int showTimer = 0;
    int hideTimer = 0;

    ~TooltipOverlay();

    static void OnShow(TooltipOverlay* self, Ctx* cx, const TickEvent* ev);
    static void OnHide(TooltipOverlay* self, Ctx* cx, const TickEvent* ev);
};

// request_show / request_hide, driven by the hover change in
// window_common.cpp rather than by each trigger element.
void TooltipRequestShow(Window* win, Str text, Bounds triggerBounds);
void TooltipRequestHide(Window* win);
// What the paint pass draws, or null when nothing is showing.
const TooltipOverlay* TooltipShowing(Window* win);
void TooltipPaint(PaintCtx* ctx, const TooltipOverlay* tip);

// Idempotent. Rust calls these from on_focus / on_blur.
void BlinkStart(App* app, Window* win, EntityId* handle);
void BlinkStop(App* app, Window* win, EntityId* handle);
// Keep it solid, then resume blinking shortly after — Rust's
// pause_blink_cursor, called from every edit and cursor movement.
void BlinkPause(App* app, Window* win, EntityId* handle);
// What a text widget asks before drawing its caret. Rust:
// blink_cursor.read(cx).visible().
bool BlinkVisible(App* app, EntityId handle);

// The same, when a Ctx is already in hand — which it is inside any Render.
inline void BlinkStart(Ctx* cx, EntityId* handle) {
    BlinkStart(cx->app, cx->win, handle);
}
inline void BlinkStop(Ctx* cx, EntityId* handle) {
    BlinkStop(cx->app, cx->win, handle);
}
inline void BlinkPause(Ctx* cx, EntityId* handle) {
    BlinkPause(cx->app, cx->win, handle);
}
inline bool BlinkVisible(Ctx* cx, EntityId handle) {
    return BlinkVisible(cx->app, handle);
}

// The input engine, when a Ctx is already in hand — which it is inside any
// Render and any listener.
inline void InputFocus(InputState* s, Ctx* cx) {
    InputFocus(s, cx->app, cx->win);
}
inline void InputBlur(InputState* s, Ctx* cx) {
    InputBlur(s, cx->app, cx->win);
}
inline void InputMoveTo(InputState* s, Ctx* cx, int offset) {
    InputMoveTo(s, cx->app, cx->win, offset);
}
inline void InputSelectAll(InputState* s, Ctx* cx) {
    InputSelectAll(s, cx->app, cx->win);
}
inline void InputClean(InputState* s, Ctx* cx) {
    InputClean(s, cx->app, cx->win);
}
inline void InputReplaceAll(InputState* s, Ctx* cx, Str value) {
    InputReplaceAll(s, cx->app, cx->win, value);
}
inline void InputInsert(InputState* s, Ctx* cx, Str value) {
    InputInsert(s, cx->app, cx->win, value);
}
inline bool InputPerform(InputState* s, Ctx* cx, InputAction action,
                         bool shift = false) {
    return InputPerform(s, cx->app, cx->win, action, shift);
}

// Open a window whose root is a view entity, the WindowOpen + cx.new pair.
Window* WindowOpenView(App* app, Str title, int dipW, int dipH, EntityId root,
                       WinOpts opts);
int AppRunView(Str title, int dipW, int dipH, EntityId root, App* app,
               WinOpts opts);

// The view a window renders, typed.
template <typename T>
T* WindowRoot(Window* win) {
    return win ? (T*)EntityGet(win->app, win->root) : nullptr;
}

// Client size in DIPs; what onRender used to receive as WinSize.
WinSize WindowSize(Window* win);

// FrameTimingCollector::collect_unseen: copy the frames drawn since *cursor
// into `out` and advance the cursor. Frames dropped from the ring while the
// caller was away are skipped. Returns how many were written.
int WindowCollectFrames(Window* win, uint64_t* cursor, FrameTiming* out,
                        int max);

// Monotonic seconds since the first call. GPUI's `Instant`, which the FPS
// readouts need at a finer resolution than GetTickCount64's ~16 ms.
double TimeNow();

App* AppNew();
void AppFree(App* app);

// Put UTF-8 text on the system clipboard.
void ClipboardSetText(Window* win, Str text);
// Take it back off. The result is arena-allocated and empty when the
// clipboard holds no text.
Str ClipboardGetText(Arena* a, Window* win);

// cx.open_url: hand a link to whatever the desktop opens links with. Rust's
// takes an `&str` and answers nothing, and so does this — a browser that
// refuses to start is not something a caller can do anything about.
void OpenUrl(Str url);

// PathPromptOptions: what the desktop's own open dialog may be pointed at.
struct PathPrompt {
    // Whether a file, a directory, or either may be chosen. Both false is a
    // dialog that can choose nothing, so it is read as `files`.
    bool files = true;
    bool directories = false;
    // The dialog's title. Rust calls it `prompt`.
    Str title = {};
};

// cx.prompt_for_paths, with one path and no task: what the user chose is
// written to `out` as a NUL-terminated path, and false comes back when they
// cancelled, when the platform has no dialog of its own (wasm), or when the
// desktop has none to offer (a Linux session with neither zenity nor
// kdialog). Rust answers a `Task<Result<Option<Vec<PathBuf>>>>` and can be
// asked for several paths at once; every caller here wants one and wants it
// where it asked, which is what the platform dialogs do anyway — they run
// their own loop until the user is done.
bool PromptForPath(Window* win, const PathPrompt& opts, char* out, int cap);

int AppRun(App* app);
Window* WindowOpen(App* app, Str title, int dipW, int dipH, WinOpts opts);
void AppSetTitle(Window* win, Str title);
void AppRequestAnim(Window* win, bool on);
// One more frame, rather than every frame. Safe to call from inside a render.
void WindowRequestAnimationFrame(Window* win);

// Collect focusable click targets from last paint for Tab cycling.
void FocusCollect(Window* win, El* root);
void IdsCollect(El* root);
int FocusNext(Window* win, int trapId, bool backward);
// Move the focus. Everything that focuses goes through here, so the
// generation a keystroke is stamped with counts every move.
// FocusHandle — crates/gpui. In GPUI a focus handle is a refcounted key into
// the window's slotmap, made with `cx.focus_handle()`, owned by whatever holds
// the state, and attached to a box with `div().track_focus(&handle)`. It has
// nothing to do with the element's name: a state that wants focus asks for a
// handle and keeps it, and the element tree picks it up again each frame.
//
// The port used to derive a focus id from an element's name — sometimes with
// an arithmetic twist to keep it clear of that name's *click* id, which is
// what `HashClickId(id) * 31 + 1` in the popover was. A handle is that done
// properly. Handles are allocated below -1000 and hashed element ids are
// positive, so the two spaces cannot meet by construction; the window chrome
// keeps -1..-4.
//
// There is no refcount and nothing is given back: an int is cheap, and the
// state that owns the handle is what keeps it meaningful.

// cx.focus_handle().
FocusHandle FocusHandleNew(App* app);
FocusHandle FocusHandleNew(Ctx* cx);
// handle.is_focused(window) / handle.focus(window) / window.focused(cx).
bool FocusHandleIsFocused(const Window* win, FocusHandle h);
// contains_focused: the handle, or anything inside the box tracking it.
bool FocusHandleContainsFocused(const Window* win, FocusHandle h);
void FocusHandleFocus(Window* win, FocusHandle h);
FocusHandle WindowFocused(const Window* win);
// The restore half of `previous_focus_handle.take()`: focus it again if the
// frame still has somewhere to put it.
bool FocusHandleRestore(Window* win, FocusHandle h);

void WindowSetFocusId(Window* win, int id);
// window.focused(cx): which element has focus, or 0. What a widget stashes
// before it takes focus for itself.
int WindowFocusedId(const Window* win);
// FocusHandle::contains_focused: focus is on this element, or inside the trap
// it hosts — which is how a container that is not itself focusable asks
// whether what it opened still holds the focus.
bool WindowFocusWithin(const Window* win, int id);
// `previous.focus(window, cx)` on a handle a widget stashed. An element that
// is no longer on screen is a handle whose view has gone, which Rust treats as
// nothing to do; answers whether focus moved.
bool WindowRestoreFocus(Window* win, int id);
// The action a keystroke resolves to for whatever has focus, and the handlers
// it is then offered to. Answers true when one of them kept it — Rust's
// `dispatch_action` plus the `cx.propagate()` that decides how far it goes.
bool WindowDispatchKeyAction(Window* win, int vk, bool shift, bool ctrl,
                             bool alt, bool platform = false);
// The action half on its own: the chord resolved against the contexts over
// the focused element, with no handler run. The matcher is stateful — a
// sequence half-finished is held on it — so a keystroke may only be resolved
// once, which is why the window does it here and hands the answer on rather
// than asking twice. `pending` comes back true when the chord began a
// sequence and belongs to nobody else.
uint32_t WindowResolveKeyAction(Window* win, int vk, bool shift, bool ctrl,
                                bool alt, bool platform, intptr_t* arg,
                                bool* pending);
// Whether the shortcut modifier is down — `secondary-` in a binding spec:
// Command on macOS, Control everywhere else. The two are separate modifiers
// now, so the code that means "the copy chord" has to say which.
constexpr bool KeySecondary(bool ctrl, bool platform) {
#if GPUI_OS_MAC
    (void)ctrl;
    return platform;
#else
    (void)platform;
    return ctrl;
#endif
}
// The same, for an action already in hand rather than one a keystroke
// resolved to. `arg` is what the action carries.
bool WindowDispatchAction(Window* win, uint32_t action, intptr_t arg = 0);
// The `El::OnKeyDown` handlers over the focused element, innermost first.
// Answers true when one of them stopped propagating.
bool WindowDispatchKeyEvent(Window* win, KeyEvent* ev);
// cx.on_action: a handler that belongs to the application rather than to any
// element. Tried after the focused element's chain has passed on the action,
// which is where Rust's App-level handlers sit too. A plain function pointer,
// since these are the framework's own and have no view to update.
using ActionFn = void (*)(Window* win, ActionEvent* ev);
void AppOnAction(uint32_t action, ActionFn fn);
void AppQuit(Window* win);
// cx.quit(): the application ends, not just this window. AppQuit closes the
// window it names and the loop ends when the last one has gone, which is the
// same thing while there is only one — a Quit row with two windows open is
// where the two part company.
void AppQuitAll(App* app);
void AppInvalidate(Window* win);
// cx.refresh_windows(): every window this app owns repaints. What a change
// with no one view behind it — the theme, the font size — asks for.
void AppRefreshWindows(App* app);
// A teardown belonging to a layer above this one, run by AppFree once the
// windows are gone. The theme registry's arena is what asked for it: it lives
// in src/ui, which gpui cannot name, and a process-wide table has to be given
// back somewhere or every ASan run reports it. Registering the same function
// twice registers it once.
void AppOnShutdown(void (*fn)());
// window.window_decorations(): whether the frame around this window is ours
// to draw. Windows and the browser answer what the window was opened with;
// macOS keeps its own controls either way. X11 only *asks* for client-side
// decorations, and a window manager that keeps its own frame anyway is what
// makes this a question — a title bar that drew its own controls under one
// would stack a second close button on top of the manager's.
bool WindowClientDecorated(Window* win);

// ─── the application menu bar ────────────────────────────────────────────
//
// cx.set_menus(app_menus()). The menus of the application itself, as opposed
// to the ones an element opens: on macOS they are the bar at the top of the
// screen, which belongs to the front application and not to any of its
// windows. A row carries an action and nothing else, the way Rust's
// `MenuItem::action` does, so choosing it runs the same handler the chord
// bound to it reaches — and the shortcut the OS shows beside the label is
// looked up in the keymap rather than spelled out here.
//
// Nothing else has a menu bar of its own to install into. The call is not
// conditional for that: an application says what its menus are once, and the
// platforms without one ignore it, which is where component::AppMenuBar
// comes in — the same menus drawn into the window.

// gpui::MenuItem. A row with no label is a separator, and a row with rows
// under it opens onto them rather than doing anything itself.
struct MenuRow {
    Str label = {};
    // The action dispatched when the row is chosen, and what it carries.
    // Zero is a row that does nothing, which is what a separator, a submenu
    // and a placeholder row all are.
    uint32_t action = 0;
    intptr_t arg = 0;
    bool separator = false;
    bool disabled = false;
    bool checked = false;
    const MenuRow* submenu = nullptr;
    int submenuN = 0;
};

// gpui::Menu: one menu of the bar, which is a name and its rows.
struct MenuDef {
    Str name = {};
    const MenuRow* items = nullptr;
    int n = 0;
};

// Whether the menus set below reach an OS menu bar. An application asks so it
// can decide whether to draw its own as well — which is what the story does,
// and what Rust decides with `cfg!(target_os = "macos")`.
bool AppHasMenuBar();
// Install these menus as the application's. Called again whenever a row's
// label or checked state changes, which is how the checked appearance and
// theme rows keep up; the platform replaces the bar wholesale.
void AppSetMenus(App* app, const MenuDef* menus, int n);
// What row `id` names, `id` being what a platform menu answers with. The
// numbering is the contract between the two halves — the selectable rows in
// preorder, from 1 — so it is worth being able to ask.
bool AppMenuRowForId(int id, uint32_t* action, intptr_t* arg);

// window.activate_window() / cx.activate(true): bring this window forward
// and the application with it, restoring it if it was minimized. What a
// click on a system notification asks for.
void AppActivate(Window* win);
void AppMinimize(Window* win);
void AppToggleMaximize(Window* win);
void AppClose(Window* win);
void AppDrag(Window* win);
bool AppIsMaximized(Window* win);
} // namespace gpui

// The entry point every example implements. The platform half of the runtime
// provides wWinMain / main and calls this, so no example spells out either.
// Global scope, so an example that says `using namespace gpui;` can define it
// without qualifying the name.
int GpuiMain(int argc, char** argv);
