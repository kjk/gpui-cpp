#include "component/Root.h"

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
    const Theme& th = ThemeNow();
    El* e = Div(a)->FlexCol()->SizeFull()->Bg(th.background);
    if (bordered) {
        e->Border(1, th.border);
    }
    if (child) {
        e->Child(child);
    }
    return e;
}

} // namespace component
} // namespace gpui
