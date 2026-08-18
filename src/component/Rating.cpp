#include "component/Rating.h"

namespace gpui {

namespace component {

struct RateBind {
    Func1<int> fn;
    int value = 0;
};
static void FireRate(RateBind* b) {
    b->fn.Call(b->value);
}

Rating* Rating::New(Ctx* cx) {
    Arena* a = cx->a;
    Rating* r = ArenaNew<Rating>(a);
    r->a = a;
    r->cx = cx;
    return r;
}
Rating* Rating::Value(int v) {
    value = v;
    return this;
}
Rating* Rating::Max(int v) {
    max = v;
    return this;
}
Rating* Rating::Disabled(bool v) {
    disabled = v;
    return this;
}
Rating* Rating::Color(Rgba c) {
    color = c;
    hasColor = true;
    return this;
}
Rating* Rating::WithSize(UiSize s) {
    size = s;
    return this;
}
Rating* Rating::OnChange(Listener fn) {
    onChange = fn;
    return this;
}

El* Rating::IntoEl() {
    const Theme& th = ThemeNow();
    El* row = Div(a)->FlexRow()->Gap(4);
    int n = max > 8 ? 8 : max;
    for (int i = 1; i <= n; i++) {
        bool on = i <= value;
        Rgba onC = hasColor ? color : th.warning;
        El* star = TextEl(a, StrL("★"))
                       ->Font(UiFontPx(size) + 4)
                       ->Fg(on ? onC : th.border);
        El* hit = Div(a)->Child(star);
        if (onChange.IsValid() && !disabled) {
            BindClick(hit, StrDup(a, fmt("star-%d", i)),
                      ListenerArg(onChange, i));
        }
        row->Child(hit);
    }
    return row;
}

} // namespace component
} // namespace gpui
