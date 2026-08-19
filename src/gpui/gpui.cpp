#include "gpui/gpui.h"
#include "base/scrollbar.h"
#include "gpui/paint.h"
#include "gpui/svg.h"

#include <math.h>

// ─── color / theme ────────────────────────────────────────────────────────

namespace gpui {

Rgba RgbaOpacity(Rgba c, float a01) {
    if (a01 < 0) {
        a01 = 0;
    }
    if (a01 > 1) {
        a01 = 1;
    }
    c.a = (uint8_t)(c.a * a01 + 0.5f);
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
    return Rgba{(uint8_t)(Clamp01(r + m) * 255.f + 0.5f),
                (uint8_t)(Clamp01(g + m) * 255.f + 0.5f),
                (uint8_t)(Clamp01(b + m) * 255.f + 0.5f),
                (uint8_t)(Clamp01(a01) * 255.f + 0.5f)};
}

const Theme& ThemeDark() {
    static Theme t;
    static bool init = false;
    if (!init) {
        t.background = Rgb(0x0a, 0x0a, 0x0a);
        t.foreground = Rgb(0xfa, 0xfa, 0xfa);
        t.border = Rgb(0x26, 0x26, 0x26);
        t.mutedFg = Rgb(0xa3, 0xa3, 0xa3);
        t.inputBorder = Rgb(0x2f, 0x2f, 0x2f);
        t.inputBg = Rgba8(0x2f, 0x2f, 0x2f, 0xb3);
        t.ring = Rgb(0x73, 0x73, 0x73);
        t.caret = Rgb(0xfa, 0xfa, 0xfa);
        t.titleBar = Rgb(0x17, 0x17, 0x17);
        t.titleBarBorder = Rgb(0x26, 0x26, 0x26);
        t.tabBar = Rgb(0x17, 0x17, 0x17);
        t.tabActiveBg = Rgb(0x0a, 0x0a, 0x0a);
        t.tabActiveFg = Rgb(0xfa, 0xfa, 0xfa);
        t.tabFg = Rgb(0xd4, 0xd4, 0xd4);
        t.tableBg = Rgb(0x0a, 0x0a, 0x0a);
        t.tableHead = Rgba8(0x17, 0x17, 0x17, 0x66);
        t.tableHeadFg = Rgb(0x52, 0x52, 0x52);
        t.tableRowBorder = Rgba8(0x26, 0x26, 0x26, 0xb3);
        t.tableEven = Rgba8(0x17, 0x17, 0x17, 0x66);
        t.progress = Rgb(0xf5, 0xf5, 0xf5);
        t.red = Rgb(0xf8, 0x71, 0x71);
        t.green = Rgb(0x4a, 0xde, 0x80);
        t.blue = Rgb(0x60, 0xa5, 0xfa);
        t.yellow = Rgb(0xfa, 0xcc, 0x15);
        t.cyan = Rgb(0x22, 0xd3, 0xee);
        t.magenta = Rgb(0xc0, 0x84, 0xfc);
        t.danger = Rgb(0xf8, 0x71, 0x71);
        t.dangerFg = Rgb(0xdc, 0x26, 0x26);
        t.secondaryHover = Rgb(0x29, 0x29, 0x29);
        t.secondaryActive = Rgb(0x21, 0x21, 0x21);
        t.secondaryFg = Rgb(0xfa, 0xfa, 0xfa);
        t.secondary = Rgb(0x26, 0x26, 0x26);
        t.muted = Rgb(0x26, 0x26, 0x26);
        t.accent = Rgb(0x26, 0x26, 0x26);
        t.primary = Rgb(0xfa, 0xfa, 0xfa);
        t.primaryFg = Rgb(0x17, 0x17, 0x17);
        t.sidebar = Rgb(0x0a, 0x0a, 0x0a);
        t.sidebarFg = Rgb(0xf5, 0xf5, 0xf5);
        t.sidebarPrimary = Rgb(0xf5, 0xf5, 0xf5);
        t.sidebarPrimaryFg = Rgb(0x0a, 0x0a, 0x0a);
        t.sidebarAccent = Rgb(0x26, 0x26, 0x26);
        t.sidebarAccentFg = Rgb(0xf5, 0xf5, 0xf5);
        t.sidebarBorder = Rgb(0x26, 0x26, 0x26);
        t.scrollbarThumb = Rgba8(0x52, 0x52, 0x52, 0xe6);
        t.info = Rgb(0x22, 0xd3, 0xee);
        t.infoFg = Rgb(0xfa, 0xfa, 0xfa);
        t.success = Rgb(0x4a, 0xde, 0x80);
        t.successFg = Rgb(0x0a, 0x0a, 0x0a);
        t.warning = Rgb(0xfa, 0xcc, 0x15);
        t.warningFg = Rgb(0x0a, 0x0a, 0x0a);
        t.skeleton = Rgb(0x26, 0x26, 0x26);
        t.overlay = Rgba8(0, 0, 0, 0x33);
        t.groupBox = Rgb(0x0a, 0x0a, 0x0a);
        t.groupBoxFg = Rgb(0xfa, 0xfa, 0xfa);
        t.descListLabel = Rgb(0x17, 0x17, 0x17);
        t.descListLabelFg = Rgb(0xf5, 0xf5, 0xf5);
        t.radius = 6;
        t.radiusLg = 8;
        init = true;
    }
    return t;
}

const Theme& ThemeLight() {
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
        t.titleBar = Rgb(0xf8, 0xf8, 0xf8);
        t.titleBarBorder = Rgb(0xe5, 0xe5, 0xe5);
        t.tabBar = Rgb(0xf5, 0xf5, 0xf5);
        t.tabActiveBg = Rgb(0xff, 0xff, 0xff);
        t.tabActiveFg = Rgb(0x17, 0x17, 0x17);
        t.tabFg = Rgb(0x40, 0x40, 0x40);
        t.tableBg = Rgb(0xff, 0xff, 0xff);
        t.tableHead = Rgb(0xfa, 0xfa, 0xfa);
        t.tableHeadFg = Rgb(0x73, 0x73, 0x73);
        t.tableRowBorder = Rgba8(0xe5, 0xe5, 0xe5, 0xb3);
        t.tableEven = Rgb(0xfa, 0xfa, 0xfa);
        t.progress = Rgb(0x17, 0x17, 0x17);
        t.red = Rgb(0xdc, 0x26, 0x26);
        t.green = Rgb(0x16, 0xa3, 0x4a);
        t.blue = Rgb(0x25, 0x63, 0xeb);
        t.yellow = Rgb(0xca, 0x8a, 0x04);
        t.cyan = Rgb(0x08, 0x91, 0xb2);
        t.magenta = Rgb(0x93, 0x33, 0xea);
        t.danger = Rgb(0xef, 0x44, 0x44);
        t.dangerFg = Rgb(0xfa, 0xfa, 0xfa);
        t.secondaryHover = Rgb(0xe5, 0xe5, 0xe5);
        t.secondaryActive = Rgb(0xd4, 0xd4, 0xd4);
        t.secondaryFg = Rgb(0x17, 0x17, 0x17);
        t.secondary = Rgb(0xe5, 0xe5, 0xe5);
        t.muted = Rgb(0xf5, 0xf5, 0xf5);
        t.accent = Rgb(0xf5, 0xf5, 0xf5);
        t.primary = Rgb(0x17, 0x17, 0x17);
        t.primaryFg = Rgb(0xfa, 0xfa, 0xfa);
        t.sidebar = Rgb(0xfa, 0xfa, 0xfa);
        t.sidebarFg = Rgb(0x17, 0x17, 0x17);
        t.sidebarPrimary = Rgb(0x17, 0x17, 0x17);
        t.sidebarPrimaryFg = Rgb(0xfa, 0xfa, 0xfa);
        t.sidebarAccent = Rgb(0xe5, 0xe5, 0xe5);
        t.sidebarAccentFg = Rgb(0x17, 0x17, 0x17);
        t.sidebarBorder = Rgb(0xe5, 0xe5, 0xe5);
        t.scrollbarThumb = Rgba8(0xa3, 0xa3, 0xa3, 0xe6);
        t.info = Rgb(0x06, 0xb6, 0xd4);
        t.infoFg = Rgb(0xfa, 0xfa, 0xfa);
        t.success = Rgb(0x22, 0xc5, 0x5e);
        t.successFg = Rgb(0xfa, 0xfa, 0xfa);
        t.warning = Rgb(0xea, 0xb3, 0x08);
        t.warningFg = Rgb(0x17, 0x17, 0x17);
        t.skeleton = Rgb(0xf5, 0xf5, 0xf5);
        t.overlay = Rgba8(0, 0, 0, 0x0d);
        t.groupBox = Rgb(0xf5, 0xf5, 0xf5);
        t.groupBoxFg = Rgb(0x17, 0x17, 0x17);
        t.descListLabel = Rgb(0xfa, 0xfa, 0xfa);
        t.descListLabelFg = Rgb(0x17, 0x17, 0x17);
        t.radius = 6;
        t.radiusLg = 8;
        init = true;
    }
    return t;
}

static ThemeMode gThemeMode = ThemeMode::Light;

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
            ->Bg(th.primary)
            ->HoverBg(RgbaMix(th.primary, th.foreground, 0.85f));
        b->Child(TextEl(a, label)->Font(14)->Fg(th.primaryFg));
    } else if (kind == BtnKind::Outline) {
        b->PadX(16)->PadY(8)->Border(1, th.border)->HoverBg(th.muted);
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

El* El::FlexRow() {
    style.dir = FlexDir::Row;
    return this;
}
El* El::FlexCol() {
    style.dir = FlexDir::Col;
    return this;
}
El* El::FlexWrap() {
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
    style.gap = v;
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
    style.align = Align::Center;
    return this;
}
El* El::ItemsStart() {
    style.align = Align::Start;
    return this;
}
El* El::ItemsEnd() {
    style.align = Align::End;
    return this;
}
El* El::JustifyBetween() {
    style.justify = Justify::SpaceBetween;
    return this;
}
El* El::JustifyCenter() {
    style.justify = Justify::Center;
    return this;
}
El* El::JustifyEnd() {
    style.justify = Justify::End;
    return this;
}
El* El::JustifyStart() {
    style.justify = Justify::Start;
    return this;
}
El* El::Bg(Rgba c) {
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
    style.overflowY = OverflowY::Hidden;
    return this;
}
El* El::ScrollY(float off) {
    style.overflowY = OverflowY::Scroll;
    scrollY = off;
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
El* El::OnMouseDown(Listener l) {
    onMouseDown = l;
    return this;
}
El* El::OnMouseUp(Listener l) {
    onMouseUp = l;
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
El* El::Caret(int off, Rgba color, float width) {
    caretOff = off;
    caretColor = color;
    caretW = width;
    return this;
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
El* El::HoverBg(Rgba c) {
    style.hoverBg = c;
    style.hasHoverBg = true;
    return this;
}
El* El::HoverFg(Rgba c) {
    style.hoverFg = c;
    style.hasHoverFg = true;
    return this;
}
El* El::FocusId(int v) {
    style.focusId = v;
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

Size MeasureText(PaintCtx* ctx, Str s, float fontSize, float maxW, bool wrap,
                 int weight, float lineH) {
    Size size = {};
    TextLayout* layout = TextMeasLayout(ctx, s, fontSize, maxW, wrap,
                                        (uint8_t)weight, lineH, &size);
    if (layout) {
        TextLayoutRelease(layout);
    }
    return size;
}

int TextIndexAt(PaintCtx* ctx, Str s, float fontSize, float maxW, bool wrap,
                float relX, float relY) {
    TextLayout* layout =
        TextMeasLayout(ctx, s, fontSize, maxW, wrap, 0, 0, nullptr);
    if (!layout) {
        return 0;
    }
    int off = TextLayoutHitPoint(layout, s, relX, relY);
    TextLayoutRelease(layout);
    return off;
}

void PaintTextRange(PaintCtx* ctx, Str s, float fontSize, float maxW, bool wrap,
                    float x, float y, int u8a, int u8b, Rgba color) {
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
        TextMeasLayout(ctx, s, fontSize, maxW, wrap, 0, 0, nullptr);
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

static float Clamp(float v, float lo, float hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static float ResolveSize(float spec, float avail, float growAsFill) {
    (void)growAsFill;
    if (spec == kFill) {
        return avail;
    }
    if (spec == kAuto) {
        return -1.f; // intrinsic
    }
    if (spec >= 0) {
        return spec;
    }
    return -1.f;
}

static void LayoutChildren(PaintCtx* ctx, El* e, float inheritFont,
                           Rgba inheritFg);

// Move a laid-out subtree without re-running layout. Positions are absolute,
// so shifting the origin shifts every descendant by the same delta; sizes are
// unaffected. This is what LayoutChildren needs once it knows where a child
// goes, and what a layout-memo hit replays.
static void TranslateSubtree(El* e, float dx, float dy) {
    for (El* c = e->first; c; c = c->next) {
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

static bool RgbaEq(Rgba a, Rgba b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

// text_color cascades in GPUI, but here a Text or an Icon resolves its own
// color when it paints. So a hovered element stamps its hover color onto the
// descendants that set none — a child with a color of its own keeps it, and
// so does its subtree.
static void StampFg(El* e, Rgba c) {
    for (El* ch = e->first; ch; ch = ch->next) {
        if (ch->style.hasColor) {
            continue;
        }
        ch->style.color = c;
        ch->style.hasColor = true;
        StampFg(ch, c);
    }
}

// Records the result of a full layout so a later call with the same inputs can
// replay it. See the memo* fields on El.
static void LayoutMemoStore(El* e, float availW, float availH,
                            float inheritFont, Rgba inheritFg) {
    e->memoAvailW = availW;
    e->memoAvailH = availH;
    e->memoFont = inheritFont;
    e->memoFg = inheritFg;
    e->memoW = e->w;
    e->memoH = e->h;
    e->memoContentW = e->contentW;
    e->memoContentH = e->contentH;
    e->memoValid = true;
}

void LayoutEl(PaintCtx* ctx, El* e, float x, float y, float availW,
              float availH, float inheritFont, Rgba inheritFg) {
    if (!e) {
        return;
    }
    // Same inputs as last time: replay the recorded sizes and slide the
    // subtree to the new origin. Everything below is a pure function of these
    // four inputs within a frame, so this is the full-fidelity answer.
    if (e->memoValid && e->memoAvailW == availW && e->memoAvailH == availH &&
        e->memoFont == inheritFont && RgbaEq(e->memoFg, inheritFg)) {
        e->w = e->memoW;
        e->h = e->memoH;
        e->contentW = e->memoContentW;
        e->contentH = e->memoContentH;
        MoveEl(e, x, y);
        return;
    }
    float font = e->style.fontSize > 0 ? e->style.fontSize : inheritFont;
    Rgba fg = e->style.hasColor ? e->style.color : inheritFg;
    // Like HoverBg, this needs a click id of its own: without one the element
    // would match hoverId 0, which means nothing is hovered.
    if (e->style.hasHoverFg && e->clickId && e->clickId == ctx->hoverId) {
        fg = e->style.hoverFg;
        StampFg(e, fg);
    }
    // font_family inherits. Pushing the flag one level down here cascades it
    // through the subtree, since every child is laid out the same way.
    if (e->style.fontMono) {
        for (El* c = e->first; c; c = c->next) {
            c->style.fontMono = true;
        }
    }

    float wSpec = ResolveSize(e->style.width, availW, e->style.flexGrow);
    float hSpec = ResolveSize(e->style.height, availH, e->style.flexGrow);

    e->x = x;
    e->y = y;

    if (e->kind == ElKind::Text) {
        float boxW = wSpec > 0 ? wSpec : availW;
        if (boxW > e->style.maxW) {
            boxW = e->style.maxW;
        }
        bool constrain = e->style.wrap || e->style.truncate;
        float measW = constrain && boxW > 0 ? boxW : 0;
        e->laidFont = font;
        e->laidMaxW = measW;
        Size text = {};
        // Same call MeasureText makes, but the shaped run is kept: paint would
        // otherwise hash and compare the whole string again to arrive at this
        // exact object. Releasing our reference is safe because a cached run
        // belongs to the cache until TextMeasEndFrame, well after paint.
        bool cached = false;
        TextLayout* tl = TextMeasLayout(ctx, e->text, font, measW,
                                        e->style.wrap, (uint8_t)ElTextWeight(e),
                                        e->style.lineHeight, &text, &cached);
        e->laidLayout = cached ? tl : nullptr;
        if (tl) {
            TextLayoutRelease(tl);
        }
        e->w = wSpec > 0 ? wSpec : Clamp(text.w, e->style.minW, e->style.maxW);
        e->h = hSpec > 0 ? hSpec : Clamp(text.h, e->style.minH, e->style.maxH);
        LayoutMemoStore(e, availW, availH, inheritFont, inheritFg);
        return;
    }
    if (e->kind == ElKind::Icon) {
        e->w = wSpec > 0 ? wSpec : 16;
        e->h = hSpec > 0 ? hSpec : 16;
        LayoutMemoStore(e, availW, availH, inheritFont, inheritFg);
        return;
    }
    if (e->kind == ElKind::Progress) {
        e->w = wSpec > 0 ? wSpec : 48;
        e->h = hSpec > 0 ? hSpec : 8;
        LayoutMemoStore(e, availW, availH, inheritFont, inheritFg);
        return;
    }

    // Container / chart: start with available or definite size
    float padX = e->style.pad.Horizontal();
    float padY = e->style.pad.Vertical();
    float innerW = (wSpec > 0 ? wSpec : availW) - padX;
    float innerH = (hSpec > 0 ? hSpec : availH) - padY;
    if (innerW < 0) {
        innerW = 0;
    }
    if (innerH < 0) {
        innerH = 0;
    }

    e->w = wSpec > 0 ? wSpec : availW;
    e->h = hSpec > 0 ? hSpec : availH;
    LayoutChildren(ctx, e, font, fg);

    // Grow() is a main-axis hint. The parent overwrites the grown size after
    // this call, so skip wrap on an auto width (typical row grow). Always
    // wrap auto height so a grow cell in a row cannot inherit the leftover
    // viewport height and swallow siblings (welcome table, feature rows).
    bool wrapW = (wSpec < 0 && e->style.flexGrow <= 0);
    bool wrapH = (hSpec < 0);
    bool resized = false;
    if (wrapW) {
        float needed = e->contentW + padX;
        float nw = Clamp(needed, e->style.minW, e->style.maxW);
        if (availW > 0 && nw > availW) {
            nw = availW;
        }
        if (nw != e->w) {
            e->w = nw;
            resized = true;
        }
    }
    // Scroll views keep the viewport height and let children overflow.
    // Wrapping to contentH would make contentH == h and hide the thumb.
    if (wrapH && e->style.overflowY != OverflowY::Scroll) {
        float needed = e->contentH + padY;
        float nh = Clamp(needed, e->style.minH, e->style.maxH);
        if (availH > 0 && nh > availH) {
            nh = availH;
        }
        if (nh != e->h) {
            e->h = nh;
            resized = true;
        }
    }
    if (resized) {
        LayoutChildren(ctx, e, font, fg);
    }
    float prevW = e->w;
    float prevH = e->h;
    e->w = Clamp(e->w, e->style.minW, e->style.maxW);
    e->h = Clamp(e->h, e->style.minH, e->style.maxH);
    if (e->w != prevW || e->h != prevH) {
        LayoutChildren(ctx, e, font, fg);
    }
    LayoutMemoStore(e, availW, availH, inheritFont, inheritFg);
}

static void PlaceOutOfFlow(PaintCtx* ctx, El* parent, El* c, float inheritFont,
                           Rgba inheritFg) {
    float ax;
    float ay;
    float aw;
    float ah;
    if (c->style.fixed) {
        ax = c->style.absLeft != kAuto ? c->style.absLeft : 0;
        ay = c->style.absTop != kAuto ? c->style.absTop : 0;
        if (c->style.width == kFill) {
            aw = ctx->viewW;
        } else if (c->style.width >= 0) {
            aw = c->style.width;
        } else {
            aw = ctx->viewW > 0 ? ctx->viewW : 10000.f;
        }
        if (c->style.height == kFill) {
            ah = ctx->viewH;
        } else if (c->style.height >= 0) {
            ah = c->style.height;
        } else {
            ah = ctx->viewH > 0 ? ctx->viewH : 10000.f;
        }
        LayoutEl(ctx, c, ax, ay, aw, ah, inheritFont, inheritFg);
        if (c->style.absRight != kAuto) {
            ax = ctx->viewW - c->style.absRight - c->w;
        }
        if (c->style.absBottom != kAuto) {
            ay = ctx->viewH - c->style.absBottom - c->h;
        }
        c->x = ax;
        c->y = ay;
        if (c->first) {
            LayoutEl(ctx, c, ax, ay, c->w, c->h, inheritFont, inheritFg);
        }
        return;
    }
    Bounds inner = parent->Bounds().Inset(parent->style.pad);
    ax = inner.x;
    ay = inner.y;
    float innerW = inner.w;
    float innerH = inner.h;
    if (innerW < 0) {
        innerW = 0;
    }
    if (innerH < 0) {
        innerH = 0;
    }
    // left(relative(f)) folds into the pixel offset before anything uses it.
    float absL = c->style.absLeft;
    float absR = c->style.absRight;
    if (c->style.absLeftRel != 0) {
        absL = (absL == kAuto ? 0.f : absL) + innerW * c->style.absLeftRel;
    }
    if (c->style.absRightRel != 0) {
        absR = (absR == kAuto ? 0.f : absR) + innerW * c->style.absRightRel;
    }
    if (c->style.width == kFill) {
        aw = innerW;
    } else if (c->style.width >= 0) {
        aw = c->style.width;
    } else if (absL != kAuto && absR != kAuto && c->style.width == kAuto) {
        // Both edges pinned and no width of its own: the box spans between
        // them, which is CSS's stretch and what left_0().right_0() asks for.
        aw = innerW - absL - absR;
        if (aw < 0) {
            aw = 0;
        }
    } else {
        aw = 10000.f;
    }
    if (c->style.height == kFill) {
        ah = innerH;
    } else if (c->style.height >= 0) {
        ah = c->style.height;
    } else if (c->style.absTop != kAuto && c->style.absBottom != kAuto &&
               c->style.height == kAuto) {
        ah = innerH - c->style.absTop - c->style.absBottom;
        if (ah < 0) {
            ah = 0;
        }
    } else {
        ah = 10000.f;
    }
    bool spanX = absL != kAuto && absR != kAuto && c->style.width == kAuto;
    bool spanY = c->style.absTop != kAuto && c->style.absBottom != kAuto &&
                 c->style.height == kAuto;
    // A span is definite for this pass only: layout runs more than once and
    // the first pass can see a parent that has no width yet, so writing the
    // result back into the style would pin the box at what that pass made of
    // it.
    float savedW = c->style.width;
    float savedH = c->style.height;
    if (spanX) {
        c->style.width = aw;
    }
    if (spanY) {
        c->style.height = ah;
    }
    LayoutEl(ctx, c, ax, ay, aw, ah, inheritFont, inheritFg);
    c->style.width = savedW;
    c->style.height = savedH;
    if (absL != kAuto) {
        ax = parent->x + absL;
    }
    if (c->style.absTop != kAuto) {
        ay = parent->y + c->style.absTop;
    }
    if (absR != kAuto && !spanX) {
        ax = parent->x + parent->w - absR - c->w;
    }
    if (c->style.absBottom != kAuto && !spanY) {
        ay = parent->y + parent->h - c->style.absBottom - c->h;
    }
    if (c->style.anchorBelow) {
        ay = parent->y + parent->h + c->style.anchorGap;
    }
    if (c->style.anchorAbove) {
        ay = parent->y - c->h - c->style.anchorGap;
    }
    if (c->style.anchorCenterX) {
        ax = parent->x + (parent->w - c->w) * 0.5f;
    }
    MoveEl(c, ax, ay);
}

static void LayoutChildren(PaintCtx* ctx, El* e, float inheritFont,
                           Rgba inheritFg) {
    float padL = e->style.pad.left;
    float padT = e->style.pad.top;
    float innerW = e->w - e->style.pad.Horizontal();
    float innerH = e->h - e->style.pad.Vertical();
    if (innerW < 0) {
        innerW = 0;
    }
    if (innerH < 0) {
        innerH = 0;
    }

    int n = 0;
    for (El* c = e->first; c; c = c->next) {
        if (!c->style.absolute) {
            n++;
        }
    }
    if (n == 0) {
        e->contentW = 0;
        e->contentH = 0;
        for (El* c = e->first; c; c = c->next) {
            if (c->style.absolute) {
                PlaceOutOfFlow(ctx, e, c, inheritFont, inheritFg);
            }
        }
        return;
    }

    // w_2_3 and friends are a fraction of this element's content box. Resolve
    // them here, once: layout re-runs a child with the width it already got,
    // and a fraction taken again would compound.
    if (innerW > 0) {
        for (El* c = e->first; c; c = c->next) {
            if (c->style.widthFrac > 0) {
                c->style.width = innerW * c->style.widthFrac;
                c->style.widthFrac = 0;
            }
        }
    }

    bool row = e->style.dir == FlexDir::Row;
    float mainAvail = row ? innerW : innerH;
    float crossAvail = row ? innerH : innerW;
    float gap = e->style.gap;
    float gaps = gap * (n - 1);
    // Overflow-y scroll: measure children with unconstrained height so
    // contentH can exceed the viewport (needed for the thumb + scroll).
    // Shrink-wrap (height:auto, no grow) must also stay unconstrained:
    // after wrap, a second LayoutChildren would otherwise pass the used
    // content height as a definite block, and H(kFill) kids (blockquote
    // bar) would expand to the whole page and hide later siblings.
    bool shrinkWrapH = e->style.height == kAuto && e->style.flexGrow <= 0 &&
                       e->style.overflowY != OverflowY::Scroll;
    bool unconstrH = e->style.overflowY == OverflowY::Scroll || shrinkWrapH;
    float childCross0 = (unconstrH && row) ? 0.f : crossAvail;
    float childMain0 = (unconstrH && !row) ? 0.f : mainAvail;

    // First pass: non-grow at the available size; grow+wrap at leftover
    // width so wrapping text is not measured as one infinite line.
    float used = 0;
    float growSum = 0;
    for (El* c = e->first; c; c = c->next) {
        if (c->style.absolute) {
            continue;
        }
        growSum += c->style.flexGrow;
        if (c->style.flexGrow > 0) {
            continue;
        }
        if (row) {
            LayoutEl(ctx, c, 0, 0, childMain0, childCross0, inheritFont,
                     inheritFg);
        } else {
            LayoutEl(ctx, c, 0, 0, childCross0, childMain0, inheritFont,
                     inheritFg);
        }
        used += row ? c->w : c->h;
    }
    used += gaps;
    float remain = mainAvail - used;
    if (remain < 0) {
        remain = 0;
    }
    for (El* c = e->first; c; c = c->next) {
        if (c->style.absolute || c->style.flexGrow <= 0) {
            continue;
        }
        float growMain = (row && c->style.wrap) ? remain : 0.f;
        if (row) {
            LayoutEl(ctx, c, 0, 0, growMain, childCross0, inheritFont,
                     inheritFg);
        } else {
            LayoutEl(ctx, c, 0, 0, childCross0, growMain, inheritFont,
                     inheritFg);
        }
        used += row ? c->w : c->h;
    }

    float leftover = mainAvail - used;
    if (leftover < 0) {
        leftover = 0;
    }

    // Second pass: assign leftover to grow children
    if (growSum > 0 && leftover > 0) {
        for (El* c = e->first; c; c = c->next) {
            if (c->style.absolute || c->style.flexGrow <= 0) {
                continue;
            }
            float extra = leftover * (c->style.flexGrow / growSum);
            if (row) {
                float w = c->w + extra;
                if (w < c->style.minW) {
                    w = c->style.minW;
                }
                LayoutEl(ctx, c, 0, 0, w, crossAvail, inheritFont, inheritFg);
                c->w = w;
            } else {
                float h = c->h + extra;
                if (h < c->style.minH) {
                    h = c->style.minH;
                }
                LayoutEl(ctx, c, 0, 0, crossAvail, h, inheritFont, inheritFg);
                c->h = h;
            }
        }
    }

    // flex_wrap: pack the children into lines no wider than the content box,
    // then stack the lines. justify applies inside a line, align across it.
    // Every story section is a wrapping row, so its content reflows instead of
    // running off the right edge.
    if (row && e->style.flexWrap && mainAvail > 0) {
        enum {
            kMaxWrapItems = 256
        };
        El* items[kMaxWrapItems];
        int nItems = 0;
        bool tooMany = false;
        for (El* c = e->first; c; c = c->next) {
            if (c->style.absolute) {
                continue;
            }
            if (nItems >= kMaxWrapItems) {
                tooMany = true;
                break;
            }
            items[nItems++] = c;
        }
        if (!tooMany) {
            float lineY = 0;
            float widest = 0;
            int i = 0;
            while (i < nItems) {
                int j = i;
                float lineW = 0;
                float lineH = 0;
                while (j < nItems) {
                    float next =
                        j > i ? lineW + gap + items[j]->w : items[j]->w;
                    if (j > i && next > mainAvail) {
                        break;
                    }
                    lineW = next;
                    if (items[j]->h > lineH) {
                        lineH = items[j]->h;
                    }
                    j++;
                }
                float x = 0;
                if (e->style.justify == Justify::Center) {
                    x = (mainAvail - lineW) * 0.5f;
                } else if (e->style.justify == Justify::End) {
                    x = mainAvail - lineW;
                }
                if (x < 0) {
                    x = 0;
                }
                for (int k = i; k < j; k++) {
                    El* c = items[k];
                    float cross = 0;
                    if (e->style.align == Align::Center) {
                        cross = (lineH - c->h) * 0.5f;
                    } else if (e->style.align == Align::End) {
                        cross = lineH - c->h;
                    }
                    float cx = e->x + padL + x;
                    float cy = e->y + padT + lineY + cross - e->scrollY;
                    MoveEl(c, cx, cy);
                    x += c->w + gap;
                }
                if (lineW > widest) {
                    widest = lineW;
                }
                lineY += lineH + gap;
                i = j;
            }
            e->contentW = widest;
            e->contentH = lineY > 0 ? lineY - gap : 0;
            for (El* c = e->first; c; c = c->next) {
                if (c->style.absolute) {
                    PlaceOutOfFlow(ctx, e, c, inheritFont, inheritFg);
                }
            }
            return;
        }
    }

    // Place
    float cursor = 0;
    if (e->style.justify == Justify::Center) {
        float total = -gap;
        for (El* c = e->first; c; c = c->next) {
            if (c->style.absolute) {
                continue;
            }
            total += (row ? c->w : c->h) + gap;
        }
        cursor = (mainAvail - total) * 0.5f;
        if (cursor < 0) {
            cursor = 0;
        }
    } else if (e->style.justify == Justify::End) {
        float total = -gap;
        for (El* c = e->first; c; c = c->next) {
            if (c->style.absolute) {
                continue;
            }
            total += (row ? c->w : c->h) + gap;
        }
        cursor = mainAvail - total;
        if (cursor < 0) {
            cursor = 0;
        }
    }

    float betweenExtra = 0;
    if (e->style.justify == Justify::SpaceBetween && n > 1) {
        float total = 0;
        for (El* c = e->first; c; c = c->next) {
            if (c->style.absolute) {
                continue;
            }
            total += row ? c->w : c->h;
        }
        // Placement adds the gap on top of betweenExtra, so the gaps have to
        // come out of the free space here or the last child lands past the
        // content box.
        float free = mainAvail - total - gaps;
        if (free > 0) {
            betweenExtra = free / (n - 1);
        }
    }

    float maxCross = 0;
    for (El* c = e->first; c; c = c->next) {
        if (c->style.absolute) {
            continue;
        }
        float cw = c->w;
        float ch = c->h;
        float cross = 0;
        if (e->style.align == Align::Center) {
            cross = ((row ? innerH : innerW) - (row ? ch : cw)) * 0.5f;
        } else if (e->style.align == Align::End) {
            cross = (row ? innerH : innerW) - (row ? ch : cw);
        } else if (e->style.align == Align::Stretch) {
            // Only stretch the cross axis when the parent already has a
            // definite size on that axis. Otherwise shrink-wrap measures
            // explode (a row header becomes as tall as the leftover column).
            if (row && e->style.height != kAuto) {
                ch = innerH;
                c->h = ch;
            } else if (!row && e->style.width != kAuto) {
                cw = innerW;
                c->w = cw;
            }
        }
        if (cross < 0) {
            cross = 0;
        }

        float cx, cy;
        if (row) {
            cx = e->x + padL + cursor;
            cy = e->y + padT + cross - e->scrollY;
        } else {
            cx = e->x + padL + cross;
            cy = e->y + padT + cursor - e->scrollY;
        }
        // The child was measured at the origin; slide it and its subtree to
        // where it actually goes. Its size is already final, so nothing below
        // has to be laid out again.
        MoveEl(c, cx, cy);

        float step = (row ? c->w : c->h) + gap + betweenExtra;
        cursor += step;
        float cr = row ? c->h : c->w;
        if (cr > maxCross) {
            maxCross = cr;
        }
    }

    // Stretch kFill / align-stretch items to the line cross size without
    // re-LayoutEl (that would treat the used height as a definite block).
    if (row && maxCross > 0) {
        for (El* c = e->first; c; c = c->next) {
            if (c->style.absolute) {
                continue;
            }
            bool fillCross =
                c->style.height == kFill ||
                (e->style.align == Align::Stretch && c->style.height == kAuto);
            if (fillCross && maxCross > c->h) {
                c->h = maxCross;
            }
        }
    }

    float intrinsicMain = 0;
    int inN = 0;
    for (El* c = e->first; c; c = c->next) {
        if (c->style.absolute) {
            continue;
        }
        if (inN) {
            intrinsicMain += gap;
        }
        intrinsicMain += row ? c->w : c->h;
        inN++;
    }
    e->contentW = row ? intrinsicMain : maxCross;
    e->contentH = row ? maxCross : intrinsicMain;

    for (El* c = e->first; c; c = c->next) {
        if (!c->style.absolute) {
            continue;
        }
        PlaceOutOfFlow(ctx, e, c, inheritFont, inheritFg);
    }
}

// ─── paint ────────────────────────────────────────────────────────────────

static void FillRound(PaintCtx* ctx, float x, float y, float w, float h,
                      float r, Rgba c) {
    CanvasFillRound(ctx, x, y, w, h, r, c);
}

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
    float keyW = wrap ? (measMaxW >= 0 ? measMaxW : (w > 0 ? w : 0)) : 0;
    TextLayout* layout = TextMeasLayout(ctx, s, fontSize, keyW, wrap,
                                        (uint8_t)weight, lineH, nullptr);
    if (!layout) {
        return;
    }
    TextLayoutDraw(ctx, layout, x, y, c, truncate);
    TextLayoutRelease(layout);
}

static void DrawIcon(PaintCtx* ctx, IconName name, float x, float y, float s,
                     Rgba c) {
    float sw = 1.6f;
    // The lucide icons are authored in a 24x24 viewBox; PX / PY map that onto
    // the element box.
    auto PX = [&](float u) { return x + u * s / 24.f; };
    auto PY = [&](float v) { return y + v * s / 24.f; };
    auto line = [&](float x1, float y1, float x2, float y2) {
        CanvasLine(ctx, PX(x1), PY(y1), PX(x2), PY(y2), sw, c);
    };
    auto dot = [&](float u, float v, float r) {
        CanvasEllipse(ctx, PX(u), PY(v), r, r, 0, c);
    };
    auto ring = [&](float u, float v, float rx, float ry) {
        CanvasEllipse(ctx, PX(u), PY(v), rx, ry, sw, c);
    };
    switch (name) {
        case IconName::WindowMinimize:
            line(5, 16, 19, 16);
            break;
        case IconName::WindowMaximize:
            DrawRoundStroke(ctx, x + s * 0.22f, y + s * 0.22f, s * 0.56f,
                            s * 0.56f, 1, sw, c);
            break;
        case IconName::WindowRestore:
            DrawRoundStroke(ctx, x + s * 0.32f, y + s * 0.18f, s * 0.46f,
                            s * 0.46f, 1, sw, c);
            DrawRoundStroke(ctx, x + s * 0.18f, y + s * 0.32f, s * 0.46f,
                            s * 0.46f, 1, sw, c);
            break;
        case IconName::WindowClose:
            line(6, 6, 18, 18);
            line(18, 6, 6, 18);
            break;
        case IconName::Cpu:
            DrawRoundStroke(ctx, x + s * 0.17f, y + s * 0.17f, s * 0.66f,
                            s * 0.66f, s * 0.08f, sw, c);
            DrawRoundStroke(ctx, x + s * 0.33f, y + s * 0.33f, s * 0.34f,
                            s * 0.34f, s * 0.04f, sw, c);
            line(12, 2, 12, 4);
            line(12, 20, 12, 22);
            line(7, 2, 7, 4);
            line(7, 20, 7, 22);
            line(17, 2, 17, 4);
            line(17, 20, 17, 22);
            line(2, 7, 4, 7);
            line(20, 7, 22, 7);
            line(2, 12, 4, 12);
            line(20, 12, 22, 12);
            line(2, 17, 4, 17);
            line(20, 17, 22, 17);
            break;
        case IconName::MemoryStick:
            DrawRoundStroke(ctx, x + s * 0.25f, y + s * 0.12f, s * 0.5f,
                            s * 0.76f, s * 0.08f, sw, c);
            line(9, 7, 15, 7);
            line(9, 11, 15, 11);
            line(9, 15, 15, 15);
            break;
        case IconName::HardDrive:
            DrawRoundStroke(ctx, x + s * 0.12f, y + s * 0.38f, s * 0.76f,
                            s * 0.36f, s * 0.08f, sw, c);
            dot(8, 14, 1.2f);
            break;
        case IconName::Battery:
        case IconName::BatteryMedium:
        case IconName::BatteryFull:
        case IconName::BatteryCharging: {
            DrawRoundStroke(ctx, x + s * 0.08f, y + s * 0.32f, s * 0.72f,
                            s * 0.36f, s * 0.06f, sw, c);
            FillRound(ctx, x + s * 0.80f, y + s * 0.42f, s * 0.08f, s * 0.16f,
                      1, c);
            float fill = 0.35f;
            if (name == IconName::BatteryFull) {
                fill = 0.85f;
            } else if (name == IconName::BatteryMedium) {
                fill = 0.5f;
            } else if (name == IconName::BatteryCharging) {
                fill = 0.6f;
            }
            FillRound(ctx, x + s * 0.14f, y + s * 0.38f, s * 0.60f * fill,
                      s * 0.24f, 1, c);
            if (name == IconName::BatteryCharging) {
                line(13, 8, 10, 13);
                line(10, 13, 14, 13);
                line(14, 13, 11, 18);
            }
            break;
        }
        case IconName::Info:
            ring(12, 12, s * 0.38f, s * 0.38f);
            line(12, 10, 12, 16);
            dot(12, 8, 1.2f);
            break;
        case IconName::X:
        case IconName::CircleX:
            if (name == IconName::CircleX) {
                ring(12, 12, s * 0.38f, s * 0.38f);
            }
            // lucide x.svg spans 6..18 of the 24 viewBox; CircleX keeps its
            // stroke inside the ring.
            if (name == IconName::CircleX) {
                line(9, 9, 15, 15);
                line(15, 9, 9, 15);
            } else {
                line(6, 6, 18, 18);
                line(18, 6, 6, 18);
            }
            break;
        case IconName::CircleCheck:
            ring(12, 12, s * 0.38f, s * 0.38f);
            line(8, 12, 11, 15);
            line(11, 15, 16, 9);
            break;
        case IconName::TriangleAlert:
            line(12, 5, 20, 19);
            line(20, 19, 4, 19);
            line(4, 19, 12, 5);
            line(12, 10, 12, 14);
            dot(12, 17, 1.1f);
            break;
        case IconName::Loader:
            line(12, 4, 12, 8);
            line(12, 16, 12, 20);
            line(4, 12, 8, 12);
            line(16, 12, 20, 12);
            line(6, 6, 9, 9);
            line(15, 15, 18, 18);
            line(18, 6, 15, 9);
            line(9, 15, 6, 18);
            break;
        case IconName::ChevronDown:
            line(6, 9, 12, 15);
            line(12, 15, 18, 9);
            break;
        case IconName::ChevronLeft:
            line(15, 6, 9, 12);
            line(9, 12, 15, 18);
            break;
        case IconName::ChevronRight:
            line(9, 6, 15, 12);
            line(15, 12, 9, 18);
            break;
        case IconName::ChevronUp:
            line(6, 15, 12, 9);
            line(12, 9, 18, 15);
            break;
        case IconName::Check:
            line(6, 12, 10, 16);
            line(10, 16, 18, 8);
            break;
        case IconName::Search:
            ring(10, 10, s * 0.22f, s * 0.22f);
            line(14, 14, 20, 20);
            break;
        case IconName::Minus:
            line(6, 12, 18, 12);
            break;
        case IconName::Plus:
            line(6, 12, 18, 12);
            line(12, 6, 12, 18);
            break;
        case IconName::Copy:
            // lucide copy: the front sheet is a 13x13 rounded square at 8,8;
            // the one behind it shows as an L along its top and left.
            DrawRoundStroke(ctx, x + s * (8.f / 24.f), y + s * (8.f / 24.f),
                            s * (13.f / 24.f), s * (13.f / 24.f), s * 0.08f, sw,
                            c);
            line(5, 15, 4, 15);
            line(4, 15, 4, 4);
            line(4, 4, 15, 4);
            line(15, 4, 15, 5);
            break;
        case IconName::Bell:
            line(12, 4, 12, 5);
            DrawRoundStroke(ctx, x + s * 0.29f, y + s * 0.25f, s * 0.42f,
                            s * 0.42f, s * 0.18f, sw, c);
            line(7, 16, 17, 16);
            dot(12, 19, 1.2f);
            break;
        case IconName::Star:
            line(12, 4, 14, 10);
            line(14, 10, 20, 10);
            line(20, 10, 15, 14);
            line(15, 14, 17, 20);
            line(17, 20, 12, 16);
            line(12, 16, 7, 20);
            line(7, 20, 9, 14);
            line(9, 14, 4, 10);
            line(4, 10, 10, 10);
            line(10, 10, 12, 4);
            break;
        case IconName::Eye:
            ring(12, 12, s * 0.38f, s * 0.22f);
            ring(12, 12, s * 0.12f, s * 0.12f);
            break;
        case IconName::Heart:
            ring(8.5f, 9, s * 0.16f, s * 0.16f);
            ring(15.5f, 9, s * 0.16f, s * 0.16f);
            line(5, 11, 12, 20);
            line(19, 11, 12, 20);
            break;
        case IconName::ArrowLeft:
            line(18, 12, 6, 12);
            line(10, 7, 6, 12);
            line(10, 17, 6, 12);
            break;
        case IconName::Building2:
            DrawRoundStroke(ctx, x + s * 0.18f, y + s * 0.18f, s * 0.38f,
                            s * 0.64f, 1, sw, c);
            DrawRoundStroke(ctx, x + s * 0.48f, y + s * 0.32f, s * 0.32f,
                            s * 0.50f, 1, sw, c);
            line(10, 22, 10, 18);
            line(8, 10, 10, 10);
            line(8, 14, 10, 14);
            line(16, 14, 18, 14);
            line(16, 18, 18, 18);
            break;
        case IconName::Asterisk:
            line(12, 5, 12, 19);
            line(6, 8, 18, 16);
            line(18, 8, 6, 16);
            break;
        case IconName::Sun:
            ring(12, 12, s * 0.16f, s * 0.16f);
            line(12, 3, 12, 6);
            line(12, 18, 12, 21);
            line(3, 12, 6, 12);
            line(18, 12, 21, 12);
            line(6, 6, 8, 8);
            line(16, 16, 18, 18);
            line(18, 6, 16, 8);
            line(8, 16, 6, 18);
            break;
        case IconName::Maximize:
            DrawRoundStroke(ctx, x + s * 0.22f, y + s * 0.22f, s * 0.56f,
                            s * 0.56f, 1, sw, c);
            break;
        default:
            break;
    }
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

    // An overlay series draws over the grid and axis the first one drew.
    if (!e->chart.overlay) {
        const float kGridDash[2] = {4.f, 2.f};
        for (int i = 0; i <= 3; i++) {
            float gy = y + plotH * (i / 4.f);
            CanvasLine(ctx, x, gy, x + w, gy, 1.f, th.border, kGridDash);
        }
        DrawLine(ctx, x, y + plotH, x + w, y + plotH, 1.f, th.border);
    }

    int n = e->chart.n;
    const float* ys = e->chart.ys;
    if (!ys || n <= 0) {
        return;
    }

    auto Xat = [&](int i) -> float {
        if (n <= 1) {
            return x + w * 0.5f;
        }
        return x + (w * (float)i / (float)(n - 1));
    };
    auto Yat = [&](float v) -> float {
        if (v < 0) {
            v = 0;
        }
        if (v > 100) {
            v = 100;
        }
        return y + 10.f + (1.f - v / 100.f) * (plotH - 10.f);
    };

    Path* area = PathNew(ctx, true);
    if (area) {
        PathMoveTo(area, Xat(0), y + plotH);
        PathLineTo(area, Xat(0), Yat(ys[0]));
        for (int i = 1; i < n; i++) {
            PathLineTo(area, Xat(i), Yat(ys[i]));
        }
        PathLineTo(area, Xat(n - 1), y + plotH);
        PathClose(area);
        PathFillGradientV(ctx, area, y, y + plotH, e->chart.fillTop,
                          e->chart.fillBot);
        PathFree(area);
    }

    if (n == 1) {
        DrawLine(ctx, x, Yat(ys[0]), x + w, Yat(ys[0]), 2.f, e->chart.stroke);
    }
    for (int i = 1; i < n; i++) {
        DrawLine(ctx, Xat(i - 1), Yat(ys[i - 1]), Xat(i), Yat(ys[i]), 2.f,
                 e->chart.stroke);
    }

    // x labels every tickMargin
    int step = e->chart.tickMargin;
    if (step < 1) {
        step = 15;
    }
    if (e->chart.overlay) {
        return;
    }
    for (int i = 0; i < n; i += step) {
        if (e->chart.labels) {
            DrawTextAt(ctx, Str(e->chart.labels[i]), Xat(i) - 16, y + plotH + 2,
                       60, 16, 10, th.mutedFg, false);
        } else {
            DrawTextAt(ctx, fmt("%ds", i), Xat(i) - 16, y + plotH + 2, 60, 16,
                       10, th.mutedFg, false);
        }
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
    CanvasFillRect(ctx, x, y, e->caretW, h, e->caretColor);
}

void PaintEl(PaintCtx* ctx, El* e) {
    PaintElNode(ctx, e, true);
    PaintOverlays(ctx, e);
}

static void PaintElNode(PaintCtx* ctx, El* e, bool skipOverlay) {
    if (!e || !ctx->rt) {
        return;
    }
    if (skipOverlay && IsOverlay(e)) {
        return;
    }
    // SliderIndicator::on_prepaint. Layout is over by the time an element
    // paints, so its box is final and the slider can map a position onto it.
    if (e->sliderBounds) {
        SliderSetBounds(e->sliderBounds, e->Bounds());
    }
    if (e->clickId || e->onClick.IsValid() || e->listener.IsValid() ||
        e->onHover.IsValid() || e->onMouseDown.IsValid() ||
        e->onMouseUp.IsValid() || e->onDragMove.IsValid() ||
        e->onMouseUpOut.IsValid() || e->drag.IsValid() ||
        e->cursor != CursorKind::Arrow || e->slider) {
        HitRect hr;
        hr.id = e->clickId;
        hr.bounds = e->Bounds();
        hr.onClick = e->onClick;
        hr.listener = e->listener;
        hr.onHover = e->onHover;
        hr.tooltip = e->style.tooltip;
        hr.onMouseDown = e->onMouseDown;
        hr.onMouseUp = e->onMouseUp;
        hr.onDragMove = e->onDragMove;
        hr.drag = e->drag;
        hr.onMouseUpOut = e->onMouseUpOut;
        hr.cursor = e->cursor;
        hr.slider = e->slider;
        hr.sliderAxis = e->sliderAxis;
        hr.input = e->input;
        ctx->hits.Append(hr);
    }
    if (e->style.overflowY == OverflowY::Scroll) {
        ScrollRect sr;
        sr.id = e->scrollId;
        sr.bounds = e->Bounds();
        sr.contentH = e->contentH;
        sr.scrollY = e->scrollY;
        sr.onScroll = e->onScroll;
        ctx->scrolls.Append(sr);
    }

    // The hover background needs a click id of its own: without one the
    // element would match hoverId 0, which means nothing is hovered.
    if (e->style.hasHoverBg && e->clickId && e->clickId == ctx->hoverId) {
        FillRound(ctx, e->x, e->y, e->w, e->h, e->style.radius,
                  e->style.hoverBg);
    } else if (e->style.hasBg) {
        FillRound(ctx, e->x, e->y, e->w, e->h, e->style.radius, e->style.bg);
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

    bool clip = e->style.overflowY != OverflowY::Visible;
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
            ctx->texts.Append(th);
            ctx->textDocLen += e->text.len + 1;
            int a = ctx->selA;
            int b = ctx->selB;
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
        if (lo >= 0 && hi > lo) {
            PaintTextRange(ctx, e->text, font,
                           e->laidMaxW > 0 ? e->laidMaxW : e->w, e->style.wrap,
                           e->x, e->y, lo, hi, e->selColor);
        }
        if (e->laidLayout) {
            TextLayoutDraw(ctx, e->laidLayout, e->x, e->y, c,
                           e->style.truncate);
        } else {
            DrawTextAt(ctx, e->text, e->x, e->y, e->w, e->h, font, c,
                       e->style.truncate, e->style.wrap, e->laidMaxW,
                       ElTextWeight(e), e->style.lineHeight);
        }
        if (clipText) {
            CanvasPopClip(ctx);
        }
        PaintCaret(ctx, e, font);
    } else if (e->kind == ElKind::Icon) {
        Rgba c = e->style.hasColor ? e->style.color : ThemeNow().foreground;
        float s = e->w > 0 ? e->w : 16;
        Str path = e->iconPath.s ? e->iconPath : IconNamePath(e->icon);
        if (!SvgDraw(ctx, path, e->x, e->y, s, c)) {
            DrawIcon(ctx, e->icon, e->x, e->y, s, c);
        }
    } else if (e->kind == ElKind::Progress) {
        const Theme& th = ThemeNow();
        Rgba track = RgbaOpacity(th.progress, 0.2f);
        FillRound(ctx, e->x, e->y, e->w, e->h, e->style.radius, track);
        float fw = e->w * (e->progress / 100.f);
        if (fw > 0) {
            FillRound(ctx, e->x, e->y, fw, e->h, e->style.radius, th.progress);
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

    for (El* c = e->first; c; c = c->next) {
        PaintElNode(ctx, c, skipOverlay);
    }

    if (clip) {
        CanvasPopClip(ctx);
    }

    if (e->style.overflowY == OverflowY::Scroll && e->contentH > e->h + 1.f &&
        e->h > 0) {
        // The same three numbers the press and drag arithmetic goes by, so
        // what is drawn and what is grabbed cannot drift apart.
        float thumbH = ScrollbarThumbSize(e->h, e->h, e->contentH);
        float thumbW = kScrollbarThumbW;
        float thumbX = e->x + e->w - thumbW - kScrollbarThumbMargin;
        float thumbY = e->y + ScrollbarThumbPos(e->h, thumbH, e->scrollY, e->h,
                                                e->contentH);
        FillRound(ctx, thumbX, thumbY, thumbW, thumbH, 3.f,
                  ThemeNow().scrollbarThumb);
    }

    if (e->style.trapId && e->style.focusId &&
        e->style.focusId == ctx->focusId) {
        // The ring sits 2 DIPs outside the element's own box.
        Bounds ring = e->Bounds().Inset(-2.f);
        DrawRoundStroke(ctx, ring.x, ring.y, ring.w, ring.h,
                        e->style.radius + 2, 2, ThemeNow().blue);
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
    Size sz = MeasureText(ctx, tip->text, 12, 280);
    // TooltipPositioner: the shared positioner's side placement, with no
    // preferred side (which prefers above), centered on the trigger, no
    // gap, and the window margin.
    Positioned at = PositionSide(tip->triggerBounds, {sz.w + 16, sz.h + 10},
                                 {ctx->viewW, ctx->viewH}, kPopupMargin,
                                 nullptr, PopupAlign::Center, 0);
    FillRound(ctx, at.bounds.x, at.bounds.y, at.bounds.w, at.bounds.h, 6,
              th.foreground);
    DrawTextAt(ctx, tip->text, at.bounds.x + 8, at.bounds.y + 5, sz.w + 4, sz.h,
               12, th.background, false);
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
static const TextHit* TextHitFind(PaintCtx* ctx, float x, float y, bool nearest,
                                  Point* outRel) {
    if (!ctx) {
        return nullptr;
    }
    const TextHit* best = nullptr;
    float bestScore = 1e9f;
    for (int i = ctx->texts.len - 1; i >= 0; i--) {
        const TextHit& h = ctx->texts[i];
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
    Point rel = {};
    const TextHit* h = TextHitFind(ctx, x, y, nearest, &rel);
    if (!h) {
        return -1;
    }
    return h->docOff + TextHitLocal(ctx, h, rel);
}

// --- word and line boundaries -------------------------------------------
// crates/base/src/text_boundary.rs. Two characters join into one word only
// when they are the same kind and that kind is Word or Whitespace, so a double
// click on a letter takes the word, one on a space takes the run of spaces,
// and one on punctuation or a CJK character takes just that character.

CharKind CharKindOf(uint32_t c) {
    bool word = c == '_' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                // Latin-1 Supplement through Latin Extended-B, the combining
                // marks, Cyrillic, and Latin Extended Additional: the ranges
                // CharacterKind::from spells out.
                (c >= 0x00C0 && c <= 0x024F) || (c >= 0x0300 && c <= 0x036F) ||
                (c >= 0x0400 && c <= 0x04FF) || (c >= 0x1E00 && c <= 0x1EFF);
    if (word) {
        return CharKind::Word;
    }
    if (c == '\n' || c == '\r') {
        return CharKind::Newline;
    }
    // Rust's char::is_whitespace, which is the Unicode White_Space property.
    bool space = c == ' ' || c == '\t' || c == 0x0B || c == 0x0C || c == 0x85 ||
                 c == 0xA0 || c == 0x1680 || (c >= 0x2000 && c <= 0x200A) ||
                 c == 0x2028 || c == 0x2029 || c == 0x202F || c == 0x205F ||
                 c == 0x3000;
    return space ? CharKind::Whitespace : CharKind::Other;
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

// clip_offset_left: into the string, then back to a character boundary.
int Utf8ClipLeft(Str s, int off) {
    if (off > s.len) {
        off = s.len;
    }
    if (off < 0) {
        off = 0;
    }
    while (off > 0 && off < s.len && ((uint8_t)s.s[off] & 0xC0) == 0x80) {
        off--;
    }
    return off;
}

// Rust stops after 128 characters in each direction; a word longer than that
// is a wall of text, not something a double click should sweep up.
static const int kWordScanMax = 128;

bool TextWordRangeAt(Str s, int off, int* outA, int* outB) {
    if (!s.s || s.len <= 0) {
        return false;
    }
    off = Utf8ClipLeft(s, off);
    if (off >= s.len) {
        return false;
    }
    uint32_t c = 0;
    int clen = Utf8At(s, off, &c);
    CharKind kind = CharKindOf(c);
    bool joins = kind == CharKind::Word || kind == CharKind::Whitespace;
    int a = off;
    int b = off + clen;
    for (int i = 0; joins && a > 0 && i < kWordScanMax; i++) {
        int prev = Utf8Prev(s, a);
        uint32_t pc = 0;
        Utf8At(s, prev, &pc);
        if (CharKindOf(pc) != kind) {
            break;
        }
        a = prev;
    }
    for (int i = 0; joins && b < s.len && i < kWordScanMax; i++) {
        uint32_t nc = 0;
        int nlen = Utf8At(s, b, &nc);
        if (CharKindOf(nc) != kind) {
            break;
        }
        b += nlen;
    }
    *outA = a;
    *outB = b;
    return true;
}

void TextLineRangeAt(Str s, int off, int* outA, int* outB) {
    *outA = 0;
    *outB = 0;
    if (!s.s || s.len <= 0) {
        return;
    }
    off = Utf8ClipLeft(s, off);
    int a = 0;
    for (int i = off - 1; i >= 0; i--) {
        if (s.s[i] == '\n') {
            a = i + 1;
            break;
        }
    }
    int b = s.len;
    for (int i = off; i < s.len; i++) {
        if (s.s[i] == '\n') {
            b = i;
            break;
        }
    }
    *outA = a;
    *outB = b;
}

bool TextMultiClickRange(PaintCtx* ctx, float x, float y, int clickCount,
                         int* outA, int* outB) {
    if (clickCount < 2) {
        return false;
    }
    Point rel = {};
    const TextHit* h = TextHitFind(ctx, x, y, false, &rel);
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
    *outA = h->docOff + a;
    *outB = h->docOff + b;
    return true;
}

int CopyTextHits(PaintCtx* ctx, int a, int b, char* out, int cap) {
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

static void CollectFocus(El* e, Window* win) {
    if (!e) {
        return;
    }
    if (e->style.focusId) {
        FocusRect fr;
        fr.id = e->style.focusId;
        fr.trapId = e->style.trapId;
        fr.bounds = e->Bounds();
        win->focusEls.Append(fr);
    }
    for (El* c = e->first; c; c = c->next) {
        CollectFocus(c, win);
    }
}

void FocusCollect(Window* win, El* root) {
    win->focusEls.Clear();
    CollectFocus(root, win);
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
        win->focusId = win->focusEls[i].id;
        return win->focusId;
    }
    return win->focusId;
}
} // namespace gpui
