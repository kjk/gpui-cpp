#include "component/WindowBorder.h"

namespace gpui {

namespace component {

WindowBorder* WindowBorder::New(Ctx* cx) {
    Arena* a = cx->a;
    WindowBorder* w = ArenaNew<WindowBorder>(a);
    w->a = a;
    w->cx = cx;
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
} // namespace gpui
