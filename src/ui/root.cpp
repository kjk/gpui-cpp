#include "ui/root.h"
#include "ui/window_border.h"

namespace gpui {

namespace component {

Root* Root::New(Ctx* cx) {
    Arena* a = cx->a;
    Root* r = ArenaNew<Root>(a);
    r->a = a;
    r->cx = cx;
    return r;
}
Root* Root::Bordered(bool v) {
    bordered = v;
    return this;
}
Root* Root::Child(El* e) {
    child = e;
    return this;
}

El* Root::IntoEl() {
    const Theme& th = cx->theme();
    El* e = Div(a)->FlexCol()->SizeFull()->Bg(th.background);
    if (child) {
        e->Child(child);
    }
    if (!bordered) {
        return e;
    }
    // Root::bordered(true) wraps the view in the window border: the shadow
    // padding a client-decorated window keeps, and the frame inside it that
    // dims while another window has the focus.
    return WindowBorder::New(cx)->Child(e)->IntoEl();
}

} // namespace component
} // namespace gpui
