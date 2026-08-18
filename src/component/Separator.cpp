#include "component/Separator.h"

namespace gpui {

namespace component {

Separator* Separator::Vertical(Ctx* cx) {
    Arena* a = cx->a;
    Separator* s = ArenaNew<Separator>(a);
    s->a = a;
    s->cx = cx;
    s->vertical = true;
    return s;
}

Separator* Separator::Horizontal(Ctx* cx) {
    Arena* a = cx->a;
    Separator* s = ArenaNew<Separator>(a);
    s->a = a;
    s->cx = cx;
    return s;
}

Separator* Separator::Dashed() {
    line = SeparatorStyle::Dashed;
    return this;
}

Separator* Separator::Label(Str s) {
    label = s;
    return this;
}

Separator* Separator::Color(Rgba c) {
    color = c;
    hasColor = true;
    return this;
}

El* Separator::IntoEl() {
    const Theme& th = cx->theme();
    Rgba c = hasColor ? color : th.border;
    El* root = Div(a)->ItemsCenter()->JustifyCenter()->Shrink0();
    if (vertical) {
        root->H(kFill)->W(label.s ? 24.f : 1.f);
        // A dashed rule is the border alone; filling the box as well would
        // paint the gaps back in.
        El* lineEl = Div(a)->W(1)->H(kFill);
        if (line == SeparatorStyle::Dashed) {
            lineEl->Dashed()->Border(1, c);
        } else {
            lineEl->Bg(c);
        }
        root->Child(lineEl);
    } else {
        root->W(kFill)->H(label.s ? 24.f : 1.f);
        El* lineEl = Div(a)->H(1)->W(kFill);
        if (line == SeparatorStyle::Dashed) {
            lineEl->Dashed()->Border(1, c);
        } else {
            lineEl->Bg(c);
        }
        root->Child(lineEl);
    }
    if (label.s) {
        root->Child(TextEl(a, label)
                        ->Font(12)
                        ->Fg(th.mutedFg)
                        ->Bg(th.background)
                        ->PadX(8));
    }
    return root;
}

} // namespace component
} // namespace gpui
