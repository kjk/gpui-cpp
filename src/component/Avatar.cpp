#include "component/Avatar.h"

namespace component {

Avatar* Avatar::New(Arena* a) {
    Avatar* v = ::New<Avatar>(a);
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
    El* fb = AvatarFallback::New(a)->W(size - 2)->H(size - 2)->ItemsCenter()->JustifyCenter()->Bg(bg)->Child(
        TextEl(a, initials)->Font(size * 0.35f)->Fg(th.foreground));
    return ::Avatar::New(a)->Size(size)->Fallback(fb)->IntoEl()->Border(1, th.border)->Radius(size * 0.5f)->ClipY();
}

} // namespace component
