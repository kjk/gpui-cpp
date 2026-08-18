#include "component/GroupBox.h"

namespace gpui {

namespace component {

GroupBox* GroupBox::New(Ctx* cx, Str title) {
    Arena* a = cx->a;
    GroupBox* g = ArenaNew<GroupBox>(a);
    g->a = a;
    g->cx = cx;
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
    const Theme& th = cx->theme();
    // Normal has neither surface nor border and no padding of its own; fill
    // and outline each take one, and pad their content.
    bool padded = filled || outline;
    El* box = Div(a)->FlexCol()->W(kFill)->Gap(padded ? 12.f : 16.f);
    if (padded) {
        box->Pad(12)->Radius(th.radius);
    }
    if (outline) {
        box->Border(1, th.border);
    } else if (filled) {
        box->Bg(th.muted);
    }
    if (title.s) {
        box->Child(TextEl(a, title)->Font(14)->Semibold()->Fg(th.mutedFg));
    }
    if (child) {
        box->Child(child);
    }
    return box;
}

} // namespace component
} // namespace gpui
