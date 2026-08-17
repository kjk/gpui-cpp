#include "component/Form.h"

namespace gpui {

namespace component {

Form* Form::New(Arena* a) {
    Form* f = ArenaNew<Form>(a);
    f->a = a;
    return f;
}
Form* Form::Field(Str label, El* control) {
    if (n < 8) {
        fields[n].label = label;
        fields[n].control = control;
        n++;
    }
    return this;
}

El* Form::IntoEl() {
    const Theme& th = ThemeNow();
    El* col = Div(a)->FlexCol()->Gap(12);
    for (int i = 0; i < n; i++) {
        El* f = Div(a)->FlexCol()->Gap(4);
        f->Child(TextEl(a, fields[i].label)->Font(12)->Fg(th.foreground));
        if (fields[i].control) {
            f->Child(fields[i].control);
        }
        col->Child(f);
    }
    return col;
}

} // namespace component
} // namespace gpui
