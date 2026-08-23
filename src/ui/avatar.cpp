#include "ui/avatar.h"

namespace gpui {

namespace component {

Avatar* Avatar::New(Ctx* cx) {
    Arena* a = cx->a;
    Avatar* v = ArenaNew<Avatar>(a);
    v->a = a;
    v->cx = cx;
    return v;
}

Str AvatarInitials(char* out, int cap, Str name) {
    // The first letter of each of the first two words.
    int n = 0;
    bool atWord = true;
    for (int i = 0; i < name.len && n < 2; i++) {
        char c = name.s[i];
        if (c == ' ') {
            atWord = true;
            continue;
        }
        if (atWord) {
            out[n++] = c;
            atWord = false;
        }
    }
    // One word only: its first two letters instead.
    if (n == 1) {
        n = 0;
        for (int i = 0; i < name.len && n < 2; i++) {
            out[n++] = name.s[i];
        }
    }
    if (n > cap - 1) {
        n = cap - 1;
    }
    for (int i = 0; i < n; i++) {
        if (out[i] >= 'a' && out[i] <= 'z') {
            out[i] = (char)(out[i] - 'a' + 'A');
        }
    }
    out[n] = 0;
    return Str{out, n};
}

Avatar* Avatar::Name(Str s) {
    char buf[8];
    Str sh = AvatarInitials(buf, (int)sizeof(buf), s);
    initials = StrDup(a, sh);
    return this;
}

Avatar* Avatar::Initials(Str s) {
    initials = s;
    return this;
}
Avatar* Avatar::Bg(Background c) {
    bg = c;
    hasBg = true;
    return this;
}
Avatar* Avatar::Size(float v) {
    size = v;
    return this;
}
// crates/ui/src/avatar/mod.rs: avatar_size + avatar_text_size. Avatars do not
// use the generic control heights.
float AvatarSizePx(UiSize s) {
    switch (s) {
        case UiSize::XSmall:
            return 16;
        case UiSize::Small:
            return 24;
        case UiSize::Large:
            return 80;
        default:
            return 48;
    }
}

static float AvatarTextPx(UiSize s) {
    switch (s) {
        case UiSize::XSmall:
            return 10.4f; // rems(0.65)
        case UiSize::Small:
            return 12; // text_xs
        case UiSize::Large:
            return 30; // text_3xl
        default:
            return 14; // text_sm
    }
}

Avatar* Avatar::WithSize(UiSize s) {
    size = AvatarSizePx(s);
    textPx = AvatarTextPx(s);
    return this;
}
Avatar* Avatar::Radius(float v) {
    radius = v;
    return this;
}
Avatar* Avatar::Border(float w, Rgba c) {
    borderW = w;
    borderC = c;
    hasBorderC = true;
    return this;
}
Avatar* Avatar::Placeholder(IconName n) {
    placeholder = n;
    return this;
}

// avatar.rs default_color: the theme's blue turned to one of 360/15 hues,
// picked by hashing the initials. Rust hashes with gpui::hash, which is
// FxHash and not something to reproduce, so the same name lands on a
// different one of the same 24 hues.
static const uint32_t kAvatarColorCount = 360 / 15;

static Rgba AvatarHue(const Theme& th, Str initials) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < initials.len; i++) {
        h ^= (uint8_t)initials.s[i];
        h *= 16777619u;
    }
    float deg = (float)((h % kAvatarColorCount) * 15);
    return RgbaWithHue(th.blue, deg / 360.f);
}

Avatar* Avatar::Src(Str url) {
    src = url;
    return this;
}

El* Avatar::IntoEl() {
    const Theme& th = cx->theme();
    float r = radius >= 0 ? radius : size * 0.5f;
    // GPUI's border sits inside the box, so the fallback fills what is left
    // of it; drawn edge to edge it would paint over the ring.
    float inset = borderW > 0 ? borderW : 0;
    float innerSize = size - inset * 2;
    bool named = initials.s && initials.len > 0;
    Background fill = th.tokens.secondary;
    Rgba text = th.mutedFg;
    if (hasBg) {
        fill = bg;
        text = th.foreground;
    } else if (named) {
        Rgba hue = AvatarHue(th, initials);
        fill = RgbaOpacity(hue, 0.2f);
        text = hue;
    }
    float txt = textPx > 0 ? textPx : size * 0.35f;
    El* inner = named ? TextEl(a, initials)->Font(txt)->Fg(text)->Semibold()
                      : IconEl(a, placeholder, size * 0.6f)->Fg(text);
    El* fb = AvatarFallback::New(cx)
                 ->W(innerSize)
                 ->H(innerSize)
                 ->ItemsCenter()
                 ->JustifyCenter()
                 ->Bg(fill)
                 ->Radius(r - inset)
                 ->Child(inner);
    // The base is opaque (bg tokens.secondary) and the fallback tint sits on
    // top, so overlapping group avatars do not show through each other.
    gpui::Avatar* base = gpui::Avatar::New(cx)->Size(size)->Fallback(fb);
    if (src.s && src.len > 0) {
        // AvatarImage::new(src).size_full().rounded_full(): the picture takes
        // the whole of the base and the fallback is not drawn at all.
        base->Image(
            AvatarImage::New(cx)
                ->W(innerSize)
                ->H(innerSize)
                ->Radius(r - inset)
                ->Child(ImageEl(a, src)->W(innerSize)->H(innerSize)->Radius(
                    r - inset)));
    }
    El* el = base->IntoEl()->Radius(r)->Bg(th.tokens.secondary);
    Rgba bd = hasBorderC ? borderC : th.border;
    if (borderW > 0) {
        el->Pad(inset)->Border(borderW, bd);
    }
    return el;
}

AvatarGroup* AvatarGroup::New(Ctx* cx) {
    AvatarGroup* g = ArenaNew<AvatarGroup>(cx->a);
    g->a = cx->a;
    g->cx = cx;
    return g;
}
AvatarGroup* AvatarGroup::Child(Avatar* av) {
    if (av && n < 16) {
        avatars[n++] = av;
    }
    return this;
}
AvatarGroup* AvatarGroup::WithSize(UiSize s) {
    size = s;
    return this;
}
AvatarGroup* AvatarGroup::Limit(int v) {
    limit = v;
    return this;
}
AvatarGroup* AvatarGroup::Ellipsis() {
    ellipsis = true;
    return this;
}

El* AvatarGroup::IntoEl() {
    float sz = AvatarSizePx(size);
    // item_ml = -avatar_size * 0.3, so each avatar past the first overlaps
    // the one before it by that much; the ⋯ chip sits ml_1 past the last.
    float step = sz - sz * 0.3f;
    int shown = n < limit ? n : limit;
    bool more = ellipsis && n > limit;
    float chipLeft = shown * step + 4;
    float w = more ? chipLeft + sz : sz + (shown > 0 ? (shown - 1) * step : 0);
    El* box = Div(a)->H(sz)->W(w);
    // flex_row_reverse: the row is built right to left, so the leftmost
    // avatar is the last child and paints over its neighbour. Absolute
    // placement gets the same stack without a reversed row or a margin.
    if (more) {
        box->Child(Avatar::New(cx)
                       // Avatar::name("⋯"): a name, so the chip is
                       // tinted and lettered like any other fallback.
                       ->Initials(StrL("⋯"))
                       ->WithSize(size)
                       ->IntoEl()
                       ->Absolute()
                       ->Left(chipLeft));
    }
    for (int i = shown - 1; i >= 0; i--) {
        box->Child(
            avatars[i]->WithSize(size)->IntoEl()->Absolute()->Left(i * step));
    }
    return box;
}

} // namespace component
} // namespace gpui
