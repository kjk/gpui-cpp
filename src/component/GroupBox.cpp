#include "component/GroupBox.h"

namespace component {

GroupBox* GroupBox::New(Arena* a, Str title) {
    GroupBox* g = ::New<GroupBox>(a);
    g->a = a;
    g->title = title;
    return g;
}
GroupBox* GroupBox::Child(El* e) {
    child = e;
    return this;
}

El* GroupBox::IntoEl() {
    const Theme& th = ThemeNow();
    El* box = Div(a)
                  ->FlexCol()
                  ->Gap(8)
                  ->Pad(12)
                  ->Radius(th.radius)
                  ->Bg(th.muted)
                  ->Border(1, th.border);
    if (title.s) {
        box->Child(TextEl(a, title)->Font(13)->Semibold()->Fg(th.foreground));
    }
    if (child) {
        box->Child(child);
    }
    return box;
}

} // namespace component
