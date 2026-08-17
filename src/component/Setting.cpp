#include "component/Setting.h"

namespace component {

Setting* Setting::New(Arena* a, Str title) {
    Setting* s = ::New<Setting>(a);
    s->a = a;
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
    const Theme& th = ThemeNow();
    El* col = Div(a)->FlexCol()->Gap(12);
    col->Child(TextEl(a, title)->Font(16)->Semibold()->Fg(th.foreground));
    for (int i = 0; i < n; i++) {
        col->Child(Div(a)
                       ->FlexRow()
                       ->ItemsCenter()
                       ->JustifyBetween()
                       ->Child(TextEl(a, items[i].label)->Font(13)->Fg(th.foreground))
                       ->Child(items[i].control ? items[i].control : Div(a)));
    }
    return col;
}

} // namespace component
