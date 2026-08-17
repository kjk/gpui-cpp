#include "component/Avatar.h"

namespace gpui {

namespace component {

Avatar* Avatar::New(Arena* a) {
    Avatar* v = ArenaNew<Avatar>(a);
    v->a = a;
    v->bg = ThemeNow().muted;
    return v;
}

Avatar* Avatar::Initials(Str s) {
    initials = s;
    return this;
}
Avatar* Avatar::Bg(Rgba c) {
    bg = c;
    return this;
}
Avatar* Avatar::Size(float v) {
    size = v;
    return this;
}

El* Avatar::IntoEl() {
    const Theme& th = ThemeNow();
    float r = size * 0.5f;
    El* inner = (initials.s && initials.len > 0)
                    ? TextEl(a, initials)->Font(size * 0.35f)->Fg(th.foreground)
                    : IconEl(a, IconName::CircleUser, size * 0.45f)
                          ->Fg(th.mutedFg);
    El* fb = AvatarFallback::New(a)
                 ->W(size)
                 ->H(size)
                 ->ItemsCenter()
                 ->JustifyCenter()
                 ->Bg(bg)
                 ->Radius(r)
                 ->Child(inner);
    return gpui::Avatar::New(a)->Size(size)->Fallback(fb)->IntoEl()->Radius(r);
}

} // namespace component
} // namespace gpui
