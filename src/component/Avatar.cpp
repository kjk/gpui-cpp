#include "component/Avatar.h"

namespace gpui {

namespace component {

Avatar* Avatar::New(Arena* a) {
    Avatar* v = ArenaNew<Avatar>(a);
    v->a = a;
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
Avatar* Avatar::WithSize(UiSize s) {
    size = UiSizePx(s);
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
    const Theme& th = ThemeNow();
    float r = radius >= 0 ? radius : size * 0.5f;
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
    El* inner =
        named ? TextEl(a, initials)->Font(size * 0.35f)->Fg(fg)->Semibold()
              : IconEl(a, placeholder, size * 0.5f)->Fg(fg);
    El* fb = AvatarFallback::New(a)
                 ->W(size)
                 ->H(size)
                 ->ItemsCenter()
                 ->JustifyCenter()
                 ->Bg(fill)
                 ->Radius(r)
                 ->Child(inner);
    El* el =
        gpui::Avatar::New(a)->Size(size)->Fallback(fb)->IntoEl()->Radius(r);
    Rgba bd = hasBorderC ? borderC : th.border;
    if (borderW > 0) {
        el->Border(borderW, bd);
    }
    return el;
}

} // namespace component
} // namespace gpui
