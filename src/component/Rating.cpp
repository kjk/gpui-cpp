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

Rating* Rating::New(Arena* a) {
    Rating* r = ArenaNew<Rating>(a);
    r->a = a;
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
Rating* Rating::OnChange(Func1<int> fn) {
    onChange = fn;
    return this;
}

El* Rating::IntoEl() {
    const Theme& th = ThemeNow();
    El* row = Div(a)->FlexRow()->Gap(4);
    int n = max > 8 ? 8 : max;
    for (int i = 1; i <= n; i++) {
        bool on = i <= value;
        El* star =
            TextEl(a, StrL("★"))->Font(18)->Fg(on ? th.warning : th.border);
        El* hit = Div(a)->Child(star);
        if (onChange.IsValid()) {
            RateBind* b = ArenaNew<RateBind>(a);
            b->fn = onChange;
            b->value = i;
            BindClick(hit, StrDup(a, fmt("star-%d", i)), MkFunc0(&FireRate, b));
        }
        row->Child(hit);
    }
    return row;
}

} // namespace component
} // namespace gpui
