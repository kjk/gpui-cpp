#include "ui/label.h"

namespace gpui {

namespace component {

Label* Label::New(Ctx* cx, Str text) {
    Arena* a = cx->a;
    Label* l = ArenaNew<Label>(a);
    l->a = a;
    l->cx = cx;
    l->text = text;
    return l;
}

Label* Label::Secondary(Str s) {
    secondary = s;
    return this;
}

Label* Label::Masked(bool v) {
    masked = v;
    return this;
}
Label* Label::Semibold() {
    semibold = true;
    return this;
}
Label* Label::Font(float px) {
    font = px;
    return this;
}

El* Label::IntoEl() {
    const Theme& th = cx->theme();
    Str shown = text;
    if (masked && text.len > 0) {
        char buf[64];
        int n = text.len < 63 ? text.len : 63;
        // ASCII bullets
        for (int i = 0; i < n; i++) {
            buf[i] = '*';
        }
        buf[n] = 0;
        shown = StrDup(a, Str(buf, n));
    }
    El* row = Div(a)->FlexRow()->ItemsCenter()->Gap(6);
    El* primary = TextEl(a, shown)->Font(font)->Fg(th.foreground);
    if (semibold) {
        primary->Semibold();
    }
    row->Child(primary);
    if (secondary.s) {
        row->Child(TextEl(a, secondary)->Font(14)->Fg(th.mutedFg));
    }
    return row;
}

} // namespace component
} // namespace gpui
