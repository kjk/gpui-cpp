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

Avatar* Avatar::Initials(Str s) {
    initials = s;
    return this;
}
Avatar* Avatar::Bg(Rgba c) {
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

static Rgba AvatarHue(Str initials) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < initials.len; i++) {
        h ^= (uint8_t)initials.s[i];
        h *= 16777619u;
    }
    static const Rgba kCols[] = {
        Rgb(0x3b, 0x82, 0xf6), Rgb(0x22, 0xc5, 0x5e), Rgb(0xa8, 0x55, 0xf7),
        Rgb(0xf9, 0x73, 0x16), Rgb(0x06, 0xb6, 0xd4), Rgb(0xec, 0x48, 0x99),
        Rgb(0xe1, 0x1d, 0x48), Rgb(0x65, 0x43, 0xd9),
    };
    return kCols[h % (sizeof(kCols) / sizeof(kCols[0]))];
}

El* Avatar::IntoEl() {
    const Theme& th = cx->theme();
    float r = radius >= 0 ? radius : size * 0.5f;
    // GPUI's border sits inside the box, so the fallback fills what is left
    // of it; drawn edge to edge it would paint over the ring.
    float inset = borderW > 0 ? borderW : 0;
    float innerSize = size - inset * 2;
    bool named = initials.s && initials.len > 0;
    Rgba fill = th.secondary;
    Rgba fg = th.mutedFg;
    if (hasBg) {
        fill = bg;
        fg = th.foreground;
    } else if (named) {
        Rgba hue = AvatarHue(initials);
        fill = RgbaOpacity(hue, 0.2f);
        fg = hue;
    }
    float txt = textPx > 0 ? textPx : size * 0.35f;
    El* inner = named ? TextEl(a, initials)->Font(txt)->Fg(fg)->Semibold()
                      : IconEl(a, placeholder, size * 0.6f)->Fg(fg);
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
    El* el = gpui::Avatar::New(cx)
                 ->Size(size)
                 ->Fallback(fb)
                 ->IntoEl()
                 ->Radius(r)
                 ->Bg(th.secondary);
    Rgba bd = hasBorderC ? borderC : th.border;
    if (borderW > 0) {
        el->Pad(inset)->Border(borderW, bd);
    }
    return el;
}

} // namespace component
} // namespace gpui
