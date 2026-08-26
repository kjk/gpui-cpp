#include "ui/spinner.h"
#include "base/motion.h"

namespace gpui {

namespace component {

// spinner.rs: `speed`, the time one turn takes. Rust's default is 0.8 s.
static const float kSpinnerPeriodMs = 800.f;

Spinner* Spinner::New(Ctx* cx) {
    Arena* a = cx->a;
    Spinner* s = ArenaNew<Spinner>(a);
    s->a = a;
    s->cx = cx;
    return s;
}

Spinner* Spinner::WithSize(UiSize s) {
    size = s;
    return this;
}
Spinner* Spinner::Size(float v) {
    px = v;
    return this;
}

Spinner* Spinner::Icon(IconName n) {
    icon = n;
    return this;
}

Spinner* Spinner::Color(Rgba c) {
    color = c;
    hasColor = true;
    return this;
}
Spinner* Spinner::Speed(float ms) {
    speed = ms;
    return this;
}
Spinner* Spinner::Ease(EaseFn fn) {
    ease = fn;
    return this;
}
Spinner* Spinner::Id(Str v) {
    id = v;
    return this;
}

El* Spinner::IntoEl() {
    const Theme& th = ThemeNow(cx->app);
    // Spinner::with_size sizes the Icon inside it, so it walks the icon
    // scale — 12 / 14 / 16 / 24 — and not the control-height one.
    float dim = px > 0 ? px : UiIconPx(size);
    // Animation::new(speed).repeat(), whose delta is a whole turn:
    // `Transformation::rotate(percentage(delta))`.
    float turn = MotionRepeat(cx, MotionId(StrL("spinner"), id),
                              speed > 0 ? speed : kSpinnerPeriodMs, ease);
    El* ic = IconEl(a, icon, dim)->Rotate(turn);
    if (hasColor) {
        ic->Fg(color);
    } else {
        ic->Fg(th.mutedFg);
    }
    return Div(a)->W(dim)->H(dim)->ItemsCenter()->JustifyCenter()->Child(ic);
}

} // namespace component
} // namespace gpui
