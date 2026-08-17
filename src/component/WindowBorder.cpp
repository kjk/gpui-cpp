#include "component/WindowBorder.h"

namespace component {

WindowBorder* WindowBorder::New(Arena* a) {
    WindowBorder* w = ::New<WindowBorder>(a);
    w->a = a;
    return w;
}
WindowBorder* WindowBorder::Child(El* e) {
    child = e;
    return this;
}

El* WindowBorder::IntoEl() {
    El* e = Div(a)->SizeFull()->Border(1, ThemeNow().border);
    if (child) {
        e->Child(child);
    }
    return e;
}

} // namespace component
