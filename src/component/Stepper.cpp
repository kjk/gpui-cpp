#include "component/Stepper.h"

namespace component {

struct StepBind {
    Func1<int> fn;
    int index = 0;
};
static void FireStep(StepBind* b) {
    b->fn.Call(b->index);
}

Stepper* Stepper::New(Arena* a) {
    Stepper* s = ::New<Stepper>(a);
    s->a = a;
    return s;
}
Stepper* Stepper::Step(Str s) {
    if (n < 8) {
        steps[n++] = s;
    }
    return this;
}
Stepper* Stepper::Current(int i) {
    current = i;
    return this;
}
Stepper* Stepper::OnChange(Func1<int> fn) {
    onChange = fn;
    return this;
}

El* Stepper::IntoEl() {
    const Theme& th = ThemeNow();
    El* row = Div(a)->FlexRow()->ItemsCenter()->Gap(8);
    for (int i = 0; i < n; i++) {
        bool on = i == current;
        bool done = i < current;
        El* dot = Div(a)->W(22)->H(22)->Radius(11)->ItemsCenter()->JustifyCenter()->Bg(on || done ? th.primary
                                                                                                  : th.secondary);
        dot->Child(TextEl(a, str::Dup(a, fmt("%d", i + 1)))->Font(11)->Fg(on || done ? th.primaryFg : th.secondaryFg));
        El* cell = Div(a)->FlexRow()->ItemsCenter()->Gap(6)->Child(dot)->Child(
            TextEl(a, steps[i])->Font(13)->Fg(th.foreground));
        if (onChange.IsValid()) {
            StepBind* b = ::New<StepBind>(a);
            b->fn = onChange;
            b->index = i;
            BindClick(cell, steps[i], MkFunc0(&FireStep, b));
        }
        row->Child(cell);
        if (i + 1 < n) {
            row->Child(Div(a)->W(24)->H(1)->Bg(th.border));
        }
    }
    return row;
}

} // namespace component
