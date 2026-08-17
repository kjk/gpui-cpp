#include "component/GroupBox.h"

namespace gpui {

namespace component {

GroupBox* GroupBox::New(Arena* a, Str title) {
    GroupBox* g = ArenaNew<GroupBox>(a);
    g->a = a;
    g->title = title;
    return g;
}
GroupBox* GroupBox::Child(El* e) {
    child = e;
    return this;
}
GroupBox* GroupBox::Outline() {
    outline = true;
    filled = false;
    return this;
}
GroupBox* GroupBox::Filled(bool v) {
    filled = v;
    return this;
}

El* GroupBox::IntoEl() {
    const Theme& th = ThemeNow();
    El* box = Div(a)->FlexCol()->Gap(8)->Pad(12)->Radius(th.radius)->Border(
        1, th.border);
    if (filled && !outline) {
        box->Bg(th.muted);
    }
    if (title.s) {
        box->Child(TextEl(a, title)->Font(13)->Semibold()->Fg(th.foreground));
    }
    if (child) {
        box->Child(child);
    }
    return box;
}

} // namespace component
} // namespace gpui
