#include "component/Text.h"

namespace gpui {

namespace component {

TextView* TextView::New(Arena* a, Str source) {
    TextView* t = ArenaNew<TextView>(a);
    t->a = a;
    t->source = source;
    return t;
}

El* TextView::IntoEl() {
    const Theme& th = ThemeNow();
    El* col = Div(a)->FlexCol()->Gap(8);
    if (!source.s) {
        return col;
    }
    const char* p = source.s;
    int i = 0;
    while (i < source.len) {
        int start = i;
        while (i < source.len && p[i] != '\n') {
            i++;
        }
        int n = i - start;
        char buf[256];
        int cpy = n > 255 ? 255 : n;
        memcpy(buf, p + start, (size_t)cpy);
        buf[cpy] = 0;
        if (buf[0] == '#' && buf[1] == ' ') {
            col->Child(TextEl(a, StrDup(a, Str(buf + 2)))
                           ->Font(18)
                           ->Semibold()
                           ->Fg(th.foreground));
        } else if (n == 0) {
            col->Child(Div(a)->H(8));
        } else {
            col->Child(TextEl(a, StrDup(a, Str(buf)))
                           ->Font(14)
                           ->Fg(th.foreground)
                           ->Wrap());
        }
        if (i < source.len && p[i] == '\n') {
            i++;
        }
    }
    return col;
}

} // namespace component
} // namespace gpui
