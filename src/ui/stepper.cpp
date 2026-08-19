#include "ui/stepper.h"

namespace gpui {

namespace component {

Stepper* Stepper::New(Ctx* cx) {
    Arena* a = cx->a;
    Stepper* s = ArenaNew<Stepper>(a);
    s->a = a;
    s->cx = cx;
    return s;
}
Stepper* Stepper::Step(Str s) {
    if (n < 8) {
        steps[n++] = s;
    }
    return this;
}
Stepper* Stepper::Step(Str s, IconName icon) {
    if (n < 8) {
        icons[n] = icon;
    }
    return Step(s);
}
Stepper* Stepper::W(float px) {
    width = px;
    return this;
}
Stepper* Stepper::Current(int i) {
    current = i;
    return this;
}
Stepper* Stepper::OnChange(Listener fn) {
    onChange = fn;
    return this;
}

El* Stepper::IntoEl() {
    const Theme& th = cx->theme();
    // Each step is a marker with its label centered under it; the connectors
    // take whatever width is left between them.
    El* root = Div(a)->FlexRow()->W(width)->ItemsStart();
    const float kDot = 24.f;
    for (int i = 0; i < n; i++) {
        bool on = i == current;
        bool done = i < current;
        Rgba fill = on || done ? th.primary : th.secondary;
        Rgba fg = on || done ? th.primaryFg : th.mutedFg;
        El* dot = Div(a)
                      ->W(kDot)
                      ->H(kDot)
                      ->Shrink0()
                      ->Radius(kDot * 0.5f)
                      ->ItemsCenter()
                      ->JustifyCenter()
                      ->Bg(fill);
        if (icons[i] != IconName::None) {
            dot->Child(IconEl(a, icons[i], 14)->Fg(fg));
        } else {
            dot->Child(TextEl(a, StrDup(a, fmt("%d", i + 1)))
                           ->Font(12)
                           ->LineHeight(1.f)
                           ->Fg(fg));
        }
        El* col = Div(a)->FlexCol()->ItemsCenter()->Gap(8)->Shrink0();
        col->Child(dot);
        col->Child(
            TextEl(a, steps[i])->Font(14)->Fg(on ? th.foreground : th.mutedFg));
        if (onChange.IsValid()) {
            BindClick(col, steps[i], ListenerArg(onChange, i));
        }
        root->Child(col);
        if (i + 1 < n) {
            root->Child(Div(a)
                            ->Grow()
                            ->H(kDot)
                            ->MinW(24)
                            ->FlexRow()
                            ->ItemsCenter()
                            ->Child(Div(a)->W(kFill)->H(1)->Bg(
                                done ? th.primary : th.border)));
        }
    }
    return root;
}

} // namespace component
} // namespace gpui
