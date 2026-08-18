#include "component/Setting.h"

namespace gpui {

namespace component {

Setting* Setting::New(Ctx* cx, Str title) {
    Arena* a = cx->a;
    Setting* s = ArenaNew<Setting>(a);
    s->a = a;
    s->cx = cx;
    s->title = title;
    return s;
}
Setting* Setting::Item(Str label, El* control) {
    if (n < 12) {
        items[n].label = label;
        items[n].control = control;
        n++;
    }
    return this;
}

El* Setting::IntoEl() {
    const Theme& th = cx->theme();
    // The group and its rows fill the width they are given, so the label sits
    // against the left edge and the control against the right.
    El* col = Div(a)->FlexCol()->Gap(12)->W(kFill);
    col->Child(TextEl(a, title)->Font(16)->Semibold()->Fg(th.foreground));
    for (int i = 0; i < n; i++) {
        col->Child(
            Div(a)
                ->FlexRow()
                ->W(kFill)
                ->ItemsCenter()
                ->JustifyBetween()
                ->Child(TextEl(a, items[i].label)->Font(13)->Fg(th.foreground))
                ->Child(items[i].control ? items[i].control : Div(a)));
    }
    return col;
}

} // namespace component
} // namespace gpui
