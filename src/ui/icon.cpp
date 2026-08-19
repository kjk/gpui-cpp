#include "ui/icon.h"

namespace gpui {

namespace component {

Icon* Icon::New(Ctx* cx, IconName name) {
    Arena* a = cx->a;
    Icon* i = ArenaNew<Icon>(a);
    i->a = a;
    i->cx = cx;
    i->name = name;
    return i;
}
Icon* Icon::Size(float v) {
    size = v;
    return this;
}
Icon* Icon::Color(Rgba c) {
    color = c;
    hasColor = true;
    return this;
}

El* Icon::IntoEl() {
    El* e = IconEl(a, name, size);
    if (hasColor) {
        e->Fg(color);
    }
    return e;
}

} // namespace component
} // namespace gpui
