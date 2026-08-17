#include "component/Icon.h"

namespace component {

Icon* Icon::New(Arena* a, IconName name) {
    Icon* i = ::New<Icon>(a);
    i->a = a;
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
