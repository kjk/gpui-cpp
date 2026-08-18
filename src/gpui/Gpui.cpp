#include "gpui/Gpui.h"
#include "gpui/Svg.h"

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

const Theme& ThemeDark() {
    static Theme t;
    static bool init = false;
    if (!init) {
        t.background = Rgb(0x0a, 0x0a, 0x0a);
        t.foreground = Rgb(0xfa, 0xfa, 0xfa);
        t.border = Rgb(0x26, 0x26, 0x26);
        t.mutedFg = Rgb(0xa3, 0xa3, 0xa3);
        t.titleBar = Rgb(0x17, 0x17, 0x17);
        t.titleBarBorder = Rgb(0x26, 0x26, 0x26);
        t.tabBar = Rgb(0x17, 0x17, 0x17);
        t.tabActiveBg = Rgb(0x0a, 0x0a, 0x0a);
        t.tabActiveFg = Rgb(0xfa, 0xfa, 0xfa);
        t.tabFg = Rgb(0xd4, 0xd4, 0xd4);
        t.tableBg = Rgb(0x0a, 0x0a, 0x0a);
        t.tableHeadFg = Rgb(0x52, 0x52, 0x52);
        t.tableRowBorder = Rgba8(0x26, 0x26, 0x26, 0xb3);
        t.tableEven = Rgba8(0x17, 0x17, 0x17, 0x66);
        t.progress = Rgb(0xf5, 0xf5, 0xf5);
        t.red = Rgb(0xf8, 0x71, 0x71);
        t.green = Rgb(0x4a, 0xde, 0x80);
        t.blue = Rgb(0x60, 0xa5, 0xfa);
        t.yellow = Rgb(0xfa, 0xcc, 0x15);
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
        t.scrollbarThumb = Rgba8(0x52, 0x52, 0x52, 0xe6);
        t.info = Rgb(0x22, 0xd3, 0xee);
        t.infoFg = Rgb(0xfa, 0xfa, 0xfa);
        t.success = Rgb(0x4a, 0xde, 0x80);
        t.successFg = Rgb(0x0a, 0x0a, 0x0a);
        t.warning = Rgb(0xfa, 0xcc, 0x15);
        t.warningFg = Rgb(0x0a, 0x0a, 0x0a);
        t.skeleton = Rgb(0x26, 0x26, 0x26);
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
        t.titleBar = Rgb(0xf8, 0xf8, 0xf8);
        t.titleBarBorder = Rgb(0xe5, 0xe5, 0xe5);
        t.tabBar = Rgb(0xf5, 0xf5, 0xf5);
        t.tabActiveBg = Rgb(0xff, 0xff, 0xff);
        t.tabActiveFg = Rgb(0x17, 0x17, 0x17);
        t.tabFg = Rgb(0x40, 0x40, 0x40);
        t.tableBg = Rgb(0xff, 0xff, 0xff);
        t.tableHeadFg = Rgb(0x73, 0x73, 0x73);
        t.tableRowBorder = Rgba8(0xe5, 0xe5, 0xe5, 0xb3);
        t.tableEven = Rgb(0xfa, 0xfa, 0xfa);
        t.progress = Rgb(0x17, 0x17, 0x17);
        t.red = Rgb(0xdc, 0x26, 0x26);
        t.green = Rgb(0x16, 0xa3, 0x4a);
        t.blue = Rgb(0x25, 0x63, 0xeb);
        t.yellow = Rgb(0xca, 0x8a, 0x04);
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
        t.scrollbarThumb = Rgba8(0xa3, 0xa3, 0xa3, 0xe6);
        t.info = Rgb(0x06, 0xb6, 0xd4);
        t.infoFg = Rgb(0xfa, 0xfa, 0xfa);
        t.success = Rgb(0x22, 0xc5, 0x5e);
        t.successFg = Rgb(0xfa, 0xfa, 0xfa);
        t.warning = Rgb(0xea, 0xb3, 0x08);
        t.warningFg = Rgb(0x17, 0x17, 0x17);
        t.skeleton = Rgb(0xf5, 0xf5, 0xf5);
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
            TextEl(a, label)->Font(selected ? 13 : 14)->Fg(th.secondaryFg));
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
    style.padL = style.padT = style.padR = style.padB = v;
    return this;
}
El* El::PadX(float v) {
    style.padL = style.padR = v;
    return this;
}
El* El::PadY(float v) {
    style.padT = style.padB = v;
    return this;
}
El* El::PadL(float v) {
    style.padL = v;
    return this;
}
El* El::PadR(float v) {
    style.padR = v;
    return this;
}
El* El::PadT(float v) {
    style.padT = v;
    return this;
}
El* El::PadB(float v) {
    style.padB = v;
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
El* El::ScrollId(int id) {
    scrollId = id;
    return this;
}
El* El::Click(int id) {
    clickId = id;
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
El* El::AnchorBelow(float gap) {
    style.absolute = true;
    style.anchorBelow = true;
    style.anchorGap = gap;
    return this;
}
El* El::Top(float v) {
    style.absTop = v;
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
El* El::FocusId(int id) {
    style.focusId = id;
    return this;
}
El* El::TrapId(int id) {
    style.trapId = id;
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

static IDWriteTextFormat* FontFor(PaintCtx* ctx, float fontSize) {
    if (fontSize >= 22.f && ctx->font24) {
        return ctx->font24;
    }
    if (fontSize >= 18.f && ctx->font20) {
        return ctx->font20;
    }
    if (fontSize <= 13.f) {
        return ctx->font12;
    }
    if (fontSize <= 15.f) {
        return ctx->font14;
    }
    return ctx->font16;
}

static void SetBrush(PaintCtx* ctx, Rgba c);

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
        case 2:
            h ^= data[1] << 8;
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
    float w = 0;
    float h = 0;
    uint32_t lastUsed = 0;
    IDWriteTextLayout* layout = nullptr;
    uint8_t wrap = 0;
    uint8_t bold = 0;
    uint8_t occupied = 0;
};

static uint32_t TextMeasHash(Str s, float fontSize, float maxW, bool wrap,
                             uint8_t weight) {
    uint32_t h = MurmurHash2(s);
    uint32_t fs = 0;
    uint32_t mw = 0;
    memcpy(&fs, &fontSize, sizeof(fs));
    memcpy(&mw, &maxW, sizeof(mw));
    h ^= fs * 0x9e3779b9u;
    h ^= mw * 0x85ebca6bu;
    if (wrap) {
        h ^= 0x165667b1u;
    }
    if (weight) {
        h ^= 0x27d4eb2fu * (uint32_t)weight;
    }
    return h;
}

static bool TextMeasKeyEq(const TextMeasSlot* sl, uint32_t hash, Str s,
                          float fontSize, float maxW, bool wrap,
                          uint8_t weight) {
    if (!sl->occupied || sl->hash != hash || sl->len != s.len) {
        return false;
    }
    if (sl->fontSize != fontSize || sl->maxW != maxW ||
        sl->wrap != (wrap ? 1 : 0) || sl->bold != weight) {
        return false;
    }
    return memeq(sl->text, s.s, s.len);
}

static uint8_t ElTextWeight(const El* e) {
    if (e->style.fontBold) {
        return 2;
    }
    if (e->style.fontSemibold) {
        return 1;
    }
    return 0;
}

static DWRITE_FONT_WEIGHT DwriteWeight(uint8_t weight) {
    if (weight >= 2) {
        return DWRITE_FONT_WEIGHT_BOLD;
    }
    if (weight == 1) {
        return DWRITE_FONT_WEIGHT_SEMI_BOLD;
    }
    return DWRITE_FONT_WEIGHT_NORMAL;
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
        sl->layout->Release();
        sl->layout = nullptr;
    }
    sl->occupied = 0;
    sl->len = 0;
}

static TextMeasSlot* TextMeasFind(TextMeasCache* c, Str s, float fontSize,
                                  float maxW, bool wrap, uint8_t weight,
                                  uint32_t* outHash) {
    float keyFont = MeasKeyFont(fontSize);
    float keyMaxW = MeasKeyMaxW(maxW, wrap);
    uint32_t hash = TextMeasHash(s, keyFont, keyMaxW, wrap, weight);
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
        if (TextMeasKeyEq(sl, hash, s, keyFont, keyMaxW, wrap, weight)) {
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
                                    float w, float h,
                                    IDWriteTextLayout* layout) {
    TextMeasCache* c = &ctx->textCache;
    float keyFont = MeasKeyFont(fontSize);
    float keyMaxW = MeasKeyMaxW(maxW, wrap);
    uint32_t hash = TextMeasHash(s, keyFont, keyMaxW, wrap, weight);
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
        if (TextMeasKeyEq(cand, hash, s, keyFont, keyMaxW, wrap, weight)) {
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
            sl->layout->Release();
        }
        layout->AddRef();
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

// Create or reuse a cached IDWriteTextLayout. Caller must Release.
static IDWriteTextLayout* TextMeasLayout(PaintCtx* ctx, Str s, float fontSize,
                                         float maxW, bool wrap, uint8_t weight,
                                         float* outW, float* outH) {
    if (outW) {
        *outW = 0;
    }
    if (outH) {
        *outH = fontSize > 0 ? fontSize * 1.25f : 16.f;
    }
    if (!ctx || !ctx->dwrite || !s.s || s.len <= 0) {
        return nullptr;
    }
    TextMeasCache* c = &ctx->textCache;
    TextMeasSlot* hit =
        TextMeasFind(c, s, fontSize, maxW, wrap, weight, nullptr);
    if (hit && hit->layout) {
        hit->lastUsed = c->frame;
        if (outW) {
            *outW = hit->w;
        }
        if (outH) {
            *outH = hit->h;
        }
        hit->layout->AddRef();
        return hit->layout;
    }
    IDWriteTextFormat* fmt = FontFor(ctx, fontSize);
    if (!fmt) {
        return nullptr;
    }
    WCHAR wbuf[2048];
    int n = MultiByteToWideChar(CP_UTF8, 0, s.s, s.len, wbuf, 2047);
    if (n <= 0) {
        return nullptr;
    }
    wbuf[n] = 0;
    IDWriteTextLayout* layout = nullptr;
    float layoutW = maxW > 0 ? maxW : 10000.f;
    HRESULT hr = ctx->dwrite->CreateTextLayout(wbuf, (UINT32)n, fmt, layoutW,
                                               4000.f, &layout);
    if (FAILED(hr) || !layout) {
        return nullptr;
    }
    DWRITE_TEXT_RANGE range = {0, (UINT32)n};
    if (fontSize > 0) {
        layout->SetFontSize(fontSize, range);
    }
    if (weight) {
        layout->SetFontWeight(DwriteWeight(weight), range);
    }
    layout->SetWordWrapping(wrap && maxW > 0 ? DWRITE_WORD_WRAPPING_WRAP
                                             : DWRITE_WORD_WRAPPING_NO_WRAP);
    DWRITE_TEXT_METRICS m = {};
    layout->GetMetrics(&m);
    if (outW) {
        *outW = m.widthIncludingTrailingWhitespace;
    }
    if (outH) {
        *outH = m.height;
    }
    TextMeasInsert(ctx, s, fontSize, maxW, wrap, weight,
                   m.widthIncludingTrailingWhitespace, m.height, layout);
    return layout;
}

void MeasureText(PaintCtx* ctx, Str s, float fontSize, float maxW, float* outW,
                 float* outH, bool wrap, int weight) {
    *outW = 0;
    *outH = fontSize > 0 ? fontSize * 1.25f : 16.f;
    IDWriteTextLayout* layout = TextMeasLayout(ctx, s, fontSize, maxW, wrap,
                                               (uint8_t)weight, outW, outH);
    if (layout) {
        layout->Release();
    }
}

static int Utf8ToWideN(Str s, WCHAR* wbuf, int cap) {
    if (!s.s || s.len <= 0 || cap < 2) {
        if (wbuf && cap > 0) {
            wbuf[0] = 0;
        }
        return 0;
    }
    int n = MultiByteToWideChar(CP_UTF8, 0, s.s, s.len, wbuf, cap - 1);
    if (n < 0) {
        n = 0;
    }
    wbuf[n] = 0;
    return n;
}

static int Utf8OffToWide(Str s, int u8off) {
    if (u8off <= 0 || !s.s) {
        return 0;
    }
    if (u8off > s.len) {
        u8off = s.len;
    }
    return MultiByteToWideChar(CP_UTF8, 0, s.s, u8off, nullptr, 0);
}

static int WideOffToUtf8(Str s, int woff) {
    if (woff <= 0 || !s.s) {
        return 0;
    }
    WCHAR wbuf[2048];
    int wn = Utf8ToWideN(s, wbuf, 2048);
    if (woff > wn) {
        woff = wn;
    }
    return WideCharToMultiByte(CP_UTF8, 0, wbuf, woff, nullptr, 0, nullptr,
                               nullptr);
}

static IDWriteTextLayout* MakeLayout(PaintCtx* ctx, Str s, float fontSize,
                                     float maxW, bool wrap, int* outWide) {
    if (outWide) {
        WCHAR wbuf[2048];
        *outWide = Utf8ToWideN(s, wbuf, 2048);
    }
    return TextMeasLayout(ctx, s, fontSize, maxW, wrap, false, nullptr,
                          nullptr);
}

int TextIndexAt(PaintCtx* ctx, Str s, float fontSize, float maxW, bool wrap,
                float relX, float relY) {
    int wn = 0;
    IDWriteTextLayout* layout = MakeLayout(ctx, s, fontSize, maxW, wrap, &wn);
    if (!layout) {
        return 0;
    }
    BOOL trailing = FALSE;
    BOOL inside = FALSE;
    DWRITE_HIT_TEST_METRICS m = {};
    layout->HitTestPoint(relX, relY, &trailing, &inside, &m);
    int wpos = (int)m.textPosition;
    if (trailing) {
        wpos += (int)m.length;
    }
    if (wpos < 0) {
        wpos = 0;
    }
    if (wpos > wn) {
        wpos = wn;
    }
    int utf8Off = WideOffToUtf8(s, wpos);
    layout->Release();
    return utf8Off;
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
    int wn = 0;
    IDWriteTextLayout* layout = MakeLayout(ctx, s, fontSize, maxW, wrap, &wn);
    if (!layout) {
        return;
    }
    int wa = Utf8OffToWide(s, u8a);
    int wb = Utf8OffToWide(s, u8b);
    if (wa > wb) {
        int t = wa;
        wa = wb;
        wb = t;
    }
    DWRITE_TEXT_METRICS tm = {};
    layout->GetMetrics(&tm);
    UINT32 lineCount = tm.lineCount;
    if (lineCount == 0) {
        layout->Release();
        return;
    }
    DWRITE_LINE_METRICS lines[32] = {};
    if (lineCount > 32) {
        lineCount = 32;
    }
    UINT32 actual = 0;
    layout->GetLineMetrics(lines, lineCount, &actual);
    UINT32 pos = 0;
    SetBrush(ctx, color);
    for (UINT32 i = 0; i < actual; i++) {
        int lineStart = (int)pos;
        int lineEnd = lineStart + (int)lines[i].length;
        int visEnd = lineEnd - (int)lines[i].newlineLength;
        pos = (UINT32)lineEnd;
        int lo = wa > lineStart ? wa : lineStart;
        int hi = wb < visEnd ? wb : visEnd;
        if (lo >= hi) {
            continue;
        }
        FLOAT x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        DWRITE_HIT_TEST_METRICS a = {}, b = {};
        layout->HitTestTextPosition((UINT32)lo, FALSE, &x0, &y0, &a);
        layout->HitTestTextPosition((UINT32)hi, FALSE, &x1, &y1, &b);
        float top = y + y0;
        float bot = top + lines[i].height;
        float left = x + x0;
        float right = x + x1;
        if (right < left) {
            float tmp = left;
            left = right;
            right = tmp;
        }
        if (hi == visEnd && lo < visEnd) {
            right = x + tm.layoutWidth;
            if (x1 > 0 && x1 + 1.f < tm.layoutWidth) {
                right = x + x1;
            }
        }
        ctx->rt->FillRectangle(D2D1::RectF(left, top, right, bot), ctx->brush);
    }
    layout->Release();
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

static int CountChildren(El* e) {
    int n = 0;
    for (El* c = e->first; c; c = c->next) {
        n++;
    }
    return n;
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

void LayoutEl(PaintCtx* ctx, El* e, float x, float y, float availW,
              float availH, float inheritFont, Rgba inheritFg) {
    if (!e) {
        return;
    }
    float font = e->style.fontSize > 0 ? e->style.fontSize : inheritFont;
    Rgba fg = e->style.hasColor ? e->style.color : inheritFg;

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
        float tw = 0, th = 0;
        MeasureText(ctx, e->text, font, measW, &tw, &th, e->style.wrap,
                    ElTextWeight(e));
        e->w = wSpec > 0 ? wSpec : Clamp(tw, e->style.minW, e->style.maxW);
        e->h = hSpec > 0 ? hSpec : Clamp(th, e->style.minH, e->style.maxH);
        return;
    }
    if (e->kind == ElKind::Icon) {
        e->w = wSpec > 0 ? wSpec : 16;
        e->h = hSpec > 0 ? hSpec : 16;
        return;
    }
    if (e->kind == ElKind::Progress) {
        e->w = wSpec > 0 ? wSpec : 48;
        e->h = hSpec > 0 ? hSpec : 8;
        return;
    }

    // Container / chart: start with available or definite size
    float padX = e->style.padL + e->style.padR;
    float padY = e->style.padT + e->style.padB;
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
    ax = parent->x + parent->style.padL;
    ay = parent->y + parent->style.padT;
    float innerW = parent->w - parent->style.padL - parent->style.padR;
    float innerH = parent->h - parent->style.padT - parent->style.padB;
    if (innerW < 0) {
        innerW = 0;
    }
    if (innerH < 0) {
        innerH = 0;
    }
    if (c->style.width == kFill) {
        aw = innerW;
    } else if (c->style.width >= 0) {
        aw = c->style.width;
    } else {
        aw = 10000.f;
    }
    if (c->style.height == kFill) {
        ah = innerH;
    } else if (c->style.height >= 0) {
        ah = c->style.height;
    } else {
        ah = 10000.f;
    }
    LayoutEl(ctx, c, ax, ay, aw, ah, inheritFont, inheritFg);
    if (c->style.absLeft != kAuto) {
        ax = parent->x + c->style.absLeft;
    }
    if (c->style.absTop != kAuto) {
        ay = parent->y + c->style.absTop;
    }
    if (c->style.absRight != kAuto) {
        ax = parent->x + parent->w - c->style.absRight - c->w;
    }
    if (c->style.absBottom != kAuto) {
        ay = parent->y + parent->h - c->style.absBottom - c->h;
    }
    if (c->style.anchorBelow) {
        ay = parent->y + parent->h + c->style.anchorGap;
    }
    c->x = ax;
    c->y = ay;
    if (c->first) {
        LayoutEl(ctx, c, ax, ay, c->w, c->h, inheritFont, inheritFg);
    }
}

static void LayoutChildren(PaintCtx* ctx, El* e, float inheritFont,
                           Rgba inheritFg) {
    float padL = e->style.padL;
    float padT = e->style.padT;
    float innerW = e->w - e->style.padL - e->style.padR;
    float innerH = e->h - e->style.padT - e->style.padB;
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
    int nGrow = 0;
    for (El* c = e->first; c; c = c->next) {
        if (c->style.absolute) {
            continue;
        }
        growSum += c->style.flexGrow;
        if (c->style.flexGrow > 0) {
            nGrow++;
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
            total += row ? c->w : c->h;
        }
        float free = mainAvail - total;
        if (free > 0) {
            betweenExtra = free / (n - 1);
        }
    }

    float maxCross = 0;
    float mainEnd = 0;
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
        float dx = cx - c->x;
        float dy = cy - c->y;
        c->x = cx;
        c->y = cy;
        // shift descendants
        if (dx != 0 || dy != 0) {
            // children already positioned relative to 0; re-layout at final pos
            if (c->first) {
                LayoutEl(ctx, c, cx, cy, c->w, c->h, inheritFont, inheritFg);
            }
        }

        float step = (row ? c->w : c->h) + gap + betweenExtra;
        cursor += step;
        mainEnd = cursor - gap - betweenExtra;
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

static void SetBrush(PaintCtx* ctx, Rgba c) {
    if (ctx->brush) {
        ctx->brush->SetColor(RgbaToD2D(c));
    }
}

static void FillRound(PaintCtx* ctx, float x, float y, float w, float h,
                      float r, Rgba c) {
    if (w <= 0 || h <= 0 || c.a == 0) {
        return;
    }
    SetBrush(ctx, c);
    D2D1_ROUNDED_RECT rr;
    rr.rect = D2D1::RectF(x, y, x + w, y + h);
    rr.radiusX = r;
    rr.radiusY = r;
    ctx->rt->FillRoundedRectangle(rr, ctx->brush);
}

static void DrawRoundStroke(PaintCtx* ctx, float x, float y, float w, float h,
                            float r, float stroke, Rgba c) {
    if (stroke <= 0 || w <= 0 || h <= 0) {
        return;
    }
    SetBrush(ctx, c);
    D2D1_ROUNDED_RECT rr;
    rr.rect = D2D1::RectF(x + stroke * 0.5f, y + stroke * 0.5f,
                          x + w - stroke * 0.5f, y + h - stroke * 0.5f);
    rr.radiusX = r;
    rr.radiusY = r;
    ctx->rt->DrawRoundedRectangle(rr, ctx->brush, stroke);
}

static void DrawLine(PaintCtx* ctx, float x1, float y1, float x2, float y2,
                     float stroke, Rgba c) {
    SetBrush(ctx, c);
    ctx->rt->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), ctx->brush,
                      stroke);
}

static void DrawTextAt(PaintCtx* ctx, Str s, float x, float y, float w, float h,
                       float fontSize, Rgba c, bool truncate, bool wrap = false,
                       float measMaxW = -1.f, int weight = 0) {
    if (!s.s || s.len <= 0 || !ctx->dwrite) {
        return;
    }
    SetBrush(ctx, c);
    float boxW = w > 0 ? w : 10000.f;
    float boxH = h > 0 ? h : 1000.f;
    D2D1_DRAW_TEXT_OPTIONS opt =
        truncate ? D2D1_DRAW_TEXT_OPTIONS_CLIP : D2D1_DRAW_TEXT_OPTIONS_NONE;
    float keyW = wrap ? (measMaxW >= 0 ? measMaxW : (w > 0 ? w : 0)) : 0;
    IDWriteTextLayout* layout = TextMeasLayout(
        ctx, s, fontSize, keyW, wrap, (uint8_t)weight, nullptr, nullptr);
    if (layout) {
        ctx->rt->DrawTextLayout(D2D1::Point2F(x, y), layout, ctx->brush, opt);
        layout->Release();
        return;
    }
    IDWriteTextFormat* fmt = FontFor(ctx, fontSize);
    if (!fmt) {
        return;
    }
    WCHAR wbuf[1024];
    int n = MultiByteToWideChar(CP_UTF8, 0, s.s, s.len, wbuf, 1023);
    if (n <= 0) {
        return;
    }
    wbuf[n] = 0;
    D2D1_RECT_F rc = D2D1::RectF(x, y, x + boxW, y + boxH);
    ctx->rt->DrawTextW(wbuf, (UINT32)n, fmt, rc, ctx->brush, opt);
}

static void DrawIcon(PaintCtx* ctx, IconName name, float x, float y, float s,
                     Rgba c) {
    SetBrush(ctx, c);
    float sw = 1.6f;
    auto P = [&](float u, float v) {
        return D2D1::Point2F(x + u * s / 24.f, y + v * s / 24.f);
    };
    auto line = [&](float x1, float y1, float x2, float y2) {
        ctx->rt->DrawLine(P(x1, y1), P(x2, y2), ctx->brush, sw, nullptr);
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
            ctx->rt
                ->FillEllipse(D2D1::Ellipse(P(8, 14), 1.2f, 1.2f), ctx->brush);
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
            ctx->rt->DrawEllipse(D2D1::Ellipse(P(12, 12), s * 0.38f, s * 0.38f),
                                 ctx->brush, sw);
            line(12, 10, 12, 16);
            ctx->rt
                ->FillEllipse(D2D1::Ellipse(P(12, 8), 1.2f, 1.2f), ctx->brush);
            break;
        case IconName::X:
        case IconName::CircleX:
            if (name == IconName::CircleX) {
                ctx->rt->DrawEllipse(
                    D2D1::Ellipse(P(12, 12), s * 0.38f, s * 0.38f), ctx->brush,
                    sw);
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
            ctx->rt->DrawEllipse(D2D1::Ellipse(P(12, 12), s * 0.38f, s * 0.38f),
                                 ctx->brush, sw);
            line(8, 12, 11, 15);
            line(11, 15, 16, 9);
            break;
        case IconName::TriangleAlert:
            line(12, 5, 20, 19);
            line(20, 19, 4, 19);
            line(4, 19, 12, 5);
            line(12, 10, 12, 14);
            ctx->rt
                ->FillEllipse(D2D1::Ellipse(P(12, 17), 1.1f, 1.1f), ctx->brush);
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
            ctx->rt->DrawEllipse(D2D1::Ellipse(P(10, 10), s * 0.22f, s * 0.22f),
                                 ctx->brush, sw);
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
            DrawRoundStroke(ctx, x + s * 0.32f, y + s * 0.18f, s * 0.42f,
                            s * 0.50f, 2, sw, c);
            DrawRoundStroke(ctx, x + s * 0.18f, y + s * 0.32f, s * 0.42f,
                            s * 0.50f, 2, sw, c);
            break;
        case IconName::Bell:
            line(12, 4, 12, 5);
            DrawRoundStroke(ctx, x + s * 0.29f, y + s * 0.25f, s * 0.42f,
                            s * 0.42f, s * 0.18f, sw, c);
            line(7, 16, 17, 16);
            ctx->rt
                ->FillEllipse(D2D1::Ellipse(P(12, 19), 1.2f, 1.2f), ctx->brush);
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
            ctx->rt->DrawEllipse(D2D1::Ellipse(P(12, 12), s * 0.38f, s * 0.22f),
                                 ctx->brush, sw);
            ctx->rt->DrawEllipse(D2D1::Ellipse(P(12, 12), s * 0.12f, s * 0.12f),
                                 ctx->brush, sw);
            break;
        case IconName::Heart:
            ctx->rt
                ->DrawEllipse(D2D1::Ellipse(P(8.5f, 9), s * 0.16f, s * 0.16f),
                              ctx->brush, sw);
            ctx->rt
                ->DrawEllipse(D2D1::Ellipse(P(15.5f, 9), s * 0.16f, s * 0.16f),
                              ctx->brush, sw);
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
            ctx->rt->DrawEllipse(D2D1::Ellipse(P(12, 12), s * 0.16f, s * 0.16f),
                                 ctx->brush, sw);
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

    // grid
    ID2D1StrokeStyle* dash = nullptr;
    D2D1_STROKE_STYLE_PROPERTIES sp = D2D1::StrokeStyleProperties();
    sp.dashStyle = D2D1_DASH_STYLE_CUSTOM;
    float dashes[2] = {4.f, 2.f};
    ctx->d2d->CreateStrokeStyle(sp, dashes, 2, &dash);
    SetBrush(ctx, th.border);
    for (int i = 0; i <= 3; i++) {
        float gy = y + plotH * (i / 4.f);
        ctx->rt->DrawLine(D2D1::Point2F(x, gy), D2D1::Point2F(x + w, gy),
                          ctx->brush, 1.f, dash);
    }
    if (dash) {
        dash->Release();
    }

    // axis
    DrawLine(ctx, x, y + plotH, x + w, y + plotH, 1.f, th.border);

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

    ID2D1PathGeometry* geom = nullptr;
    ctx->d2d->CreatePathGeometry(&geom);
    if (geom) {
        ID2D1GeometrySink* sink = nullptr;
        if (SUCCEEDED(geom->Open(&sink))) {
            sink->BeginFigure(D2D1::Point2F(Xat(0), y + plotH),
                              D2D1_FIGURE_BEGIN_FILLED);
            sink->AddLine(D2D1::Point2F(Xat(0), Yat(ys[0])));
            for (int i = 1; i < n; i++) {
                sink->AddLine(D2D1::Point2F(Xat(i), Yat(ys[i])));
            }
            sink->AddLine(D2D1::Point2F(Xat(n - 1), y + plotH));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            sink->Close();
            sink->Release();
        }

        ID2D1GradientStopCollection* stops = nullptr;
        D2D1_GRADIENT_STOP gs[2];
        gs[0].position = 0.f;
        gs[0].color = RgbaToD2D(e->chart.fillTop);
        gs[1].position = 1.f;
        gs[1].color = RgbaToD2D(e->chart.fillBot);
        ctx->rt->CreateGradientStopCollection(gs, 2, &stops);
        bool filled = false;
        if (stops) {
            ID2D1LinearGradientBrush* gb = nullptr;
            ctx->rt->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(
                    D2D1::Point2F(x, y), D2D1::Point2F(x, y + plotH)),
                stops, &gb);
            if (gb) {
                ctx->rt->FillGeometry(geom, gb);
                gb->Release();
                filled = true;
            }
            stops->Release();
        }
        if (!filled) {
            SetBrush(ctx, e->chart.fillTop);
            ctx->rt->FillGeometry(geom, ctx->brush);
        }
        geom->Release();
    }

    SetBrush(ctx, e->chart.stroke);
    if (n == 1) {
        ctx->rt->DrawLine(D2D1::Point2F(x, Yat(ys[0])),
                          D2D1::Point2F(x + w, Yat(ys[0])), ctx->brush, 2.f);
    }
    for (int i = 1; i < n; i++) {
        ctx->rt->DrawLine(D2D1::Point2F(Xat(i - 1), Yat(ys[i - 1])),
                          D2D1::Point2F(Xat(i), Yat(ys[i])), ctx->brush, 2.f);
    }

    // x labels every tickMargin
    int step = e->chart.tickMargin;
    if (step < 1) {
        step = 15;
    }
    for (int i = 0; i < n; i += step) {
        TempStr lab = fmt("%ds", i);
        DrawTextAt(ctx, lab, Xat(i) - 8, y + plotH + 2, 40, 16, 10, th.mutedFg,
                   false);
    }
}

static void PaintElNode(PaintCtx* ctx, El* e, bool skipFixed);

static void PaintFixedOverlays(PaintCtx* ctx, El* e) {
    if (!e) {
        return;
    }
    if (e->style.fixed) {
        PaintElNode(ctx, e, false);
        return;
    }
    for (El* c = e->first; c; c = c->next) {
        PaintFixedOverlays(ctx, c);
    }
}

void PaintEl(PaintCtx* ctx, El* e) {
    PaintElNode(ctx, e, true);
    PaintFixedOverlays(ctx, e);
}

static void PaintElNode(PaintCtx* ctx, El* e, bool skipFixed) {
    if (!e || !ctx->rt) {
        return;
    }
    if (skipFixed && e->style.fixed) {
        return;
    }
    if (e->clickId || e->onClick.IsValid() || e->listener.IsValid()) {
        HitRect hr;
        hr.id = e->clickId;
        hr.x = e->x;
        hr.y = e->y;
        hr.w = e->w;
        hr.h = e->h;
        hr.onClick = e->onClick;
        hr.listener = e->listener;
        ctx->hits.Append(hr);
    }
    if (e->style.overflowY == OverflowY::Scroll) {
        ScrollRect sr;
        sr.id = e->scrollId;
        sr.x = e->x;
        sr.y = e->y;
        sr.w = e->w;
        sr.h = e->h;
        sr.contentH = e->contentH;
        ctx->scrolls.Append(sr);
    }

    if (e->style.hasHoverBg &&
        ((e->clickId && e->clickId == ctx->hoverId) ||
         (e->onClick.IsValid() && e->clickId == ctx->hoverId))) {
        FillRound(ctx, e->x, e->y, e->w, e->h, e->style.radius,
                  e->style.hoverBg);
    } else if (e->style.hasBg) {
        FillRound(ctx, e->x, e->y, e->w, e->h, e->style.radius, e->style.bg);
    }
    if (e->style.border > 0) {
        if (e->style.borderDashed) {
            ID2D1StrokeStyle* dash = nullptr;
            D2D1_STROKE_STYLE_PROPERTIES sp = D2D1::StrokeStyleProperties();
            sp.dashStyle = D2D1_DASH_STYLE_DASH;
            ctx->d2d->CreateStrokeStyle(sp, nullptr, 0, &dash);
            SetBrush(ctx, e->style.borderColor);
            D2D1_ROUNDED_RECT rr;
            rr.rect = D2D1::RectF(e->x, e->y, e->x + e->w, e->y + e->h);
            rr.radiusX = e->style.radius;
            rr.radiusY = e->style.radius;
            ctx->rt
                ->DrawRoundedRectangle(rr, ctx->brush, e->style.border, dash);
            if (dash) {
                dash->Release();
            }
        } else {
            DrawRoundStroke(ctx, e->x, e->y, e->w, e->h, e->style.radius,
                            e->style.border, e->style.borderColor);
        }
    }
    if (e->style.borderT > 0) {
        DrawLine(ctx, e->x, e->y, e->x + e->w, e->y, e->style.borderT,
                 e->style.borderColor);
    }
    if (e->style.borderB > 0) {
        DrawLine(ctx, e->x, e->y + e->h, e->x + e->w, e->y + e->h,
                 e->style.borderB, e->style.borderColor);
    }

    bool clip = e->style.overflowY != OverflowY::Visible;
    if (clip) {
        ctx->rt->PushAxisAlignedClip(
            D2D1::RectF(e->x, e->y, e->x + e->w, e->y + e->h),
            D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    }

    if (e->kind == ElKind::Text) {
        float font = e->laidFont > 0
                         ? e->laidFont
                         : (e->style.fontSize > 0 ? e->style.fontSize : 14.f);
        Rgba c = e->style.hasColor ? e->style.color : ThemeNow().foreground;
        int lo = e->selLo;
        int hi = e->selHi;
        if (e->selectable && e->text.s) {
            int docOff = ctx->textDocLen;
            TextHit th;
            th.x = e->x;
            th.y = e->y;
            th.w = e->w;
            th.h = e->h;
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
        if (lo >= 0 && hi > lo) {
            PaintTextRange(ctx, e->text, font,
                           e->laidMaxW > 0 ? e->laidMaxW : e->w, e->style.wrap,
                           e->x, e->y, lo, hi, Rgba8(0x6b, 0xb3, 0xf0, 90));
        }
        DrawTextAt(ctx, e->text, e->x, e->y, e->w, e->h, font, c,
                   e->style.truncate, e->style.wrap, e->laidMaxW,
                   ElTextWeight(e));
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
    if (e->customPaint) {
        e->customPaint(ctx, e, e->customUser);
    }

    for (El* c = e->first; c; c = c->next) {
        PaintElNode(ctx, c, skipFixed);
    }

    if (clip) {
        ctx->rt->PopAxisAlignedClip();
    }

    if (e->style.overflowY == OverflowY::Scroll && e->contentH > e->h + 1.f &&
        e->h > 0) {
        float view = e->h;
        float content = e->contentH;
        float thumbH = view * (view / content);
        if (thumbH < 48.f) {
            thumbH = 48.f;
        }
        if (thumbH > view) {
            thumbH = view;
        }
        float maxScroll = content - view;
        if (maxScroll < 1.f) {
            maxScroll = 1.f;
        }
        float t = e->scrollY / maxScroll;
        if (t < 0) {
            t = 0;
        }
        if (t > 1) {
            t = 1;
        }
        float thumbW = 6.f;
        float thumbX = e->x + e->w - thumbW - 4.f;
        float thumbY = e->y + t * (view - thumbH);
        FillRound(ctx, thumbX, thumbY, thumbW, thumbH, 3.f,
                  ThemeNow().scrollbarThumb);
    }

    if (e->style.trapId && e->style.focusId &&
        e->style.focusId == ctx->focusId) {
        DrawRoundStroke(ctx, e->x - 2, e->y - 2, e->w + 4, e->h + 4,
                        e->style.radius + 2, 2, ThemeNow().blue);
    }
    if (e->style.tooltip.s && e->clickId && e->clickId == ctx->hoverId) {
        const Theme& th = ThemeNow();
        float tw = 0, thh = 0;
        MeasureText(ctx, e->style.tooltip, 12, 280, &tw, &thh);
        float pw = tw + 16, ph = thh + 10;
        float px = e->x;
        float py = e->y - ph - 8;
        if (py < 4) {
            py = e->y + e->h + 8;
        }
        FillRound(ctx, px, py, pw, ph, 6, th.foreground);
        DrawTextAt(ctx, e->style.tooltip, px + 8, py + 5, tw + 4, thh, 12,
                   th.background, false);
    }
}

const HitRect* HitTestRect(PaintCtx* ctx, float x, float y) {
    for (int i = ctx->hits.len - 1; i >= 0; i--) {
        const HitRect& h = ctx->hits[i];
        if (x >= h.x && x < h.x + h.w && y >= h.y && y < h.y + h.h) {
            return &ctx->hits[i];
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
        if (x >= s.x && x < s.x + s.w && y >= s.y && y < s.y + s.h) {
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

int TextHitOffsetAt(PaintCtx* ctx, float x, float y, bool nearest) {
    if (!ctx) {
        return -1;
    }
    const TextHit* best = nullptr;
    float bestScore = 1e9f;
    for (int i = ctx->texts.len - 1; i >= 0; i--) {
        const TextHit& h = ctx->texts[i];
        if (x >= h.x && x < h.x + h.w && y >= h.y && y < h.y + h.h) {
            best = &h;
            nearest = false;
            break;
        }
        if (!nearest) {
            continue;
        }
        float dy = DistToInterval(y, h.y, h.y + h.h);
        float dx = DistToInterval(x, h.x, h.x + h.w);
        float score = dy * 1000.f + dx;
        if (score < bestScore) {
            bestScore = score;
            best = &h;
        }
    }
    if (!best || !best->text.s) {
        return -1;
    }
    float relX = x - best->x;
    float relY = y - best->y;
    if (nearest) {
        if (relX < 0) {
            relX = 0;
        }
        if (relY < 0) {
            relY = 0;
        }
        if (relX > best->w) {
            relX = best->w;
        }
        if (relY > best->h) {
            relY = best->h;
        }
    }
    int local = TextIndexAt(ctx, best->text, best->font,
                            best->maxW > 0 ? best->maxW : best->w, best->wrap,
                            relX, relY);
    if (local < 0) {
        local = 0;
    }
    if (local > best->text.len) {
        local = best->text.len;
    }
    return best->docOff + local;
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
        fr.x = e->x;
        fr.y = e->y;
        fr.w = e->w;
        fr.h = e->h;
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
