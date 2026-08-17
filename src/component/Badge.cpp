#include "component/Badge.h"

namespace gpui {

namespace component {

Badge* Badge::New(Ctx* cx) {
    Arena* a = cx->a;
    Badge* b = ArenaNew<Badge>(a);
    b->a = a;
    b->cx = cx;
    return b;
}

Badge* Badge::Count(int n) {
    count = n;
    kind = BadgeKind::Number;
    return this;
}
Badge* Badge::Max(int n) {
    max = n;
    return this;
}
Badge* Badge::Dot() {
    kind = BadgeKind::Dot;
    return this;
}
Badge* Badge::Icon(IconName n) {
    icon = n;
    kind = BadgeKind::Icon;
    return this;
}
Badge* Badge::Color(Rgba c) {
    color = c;
    hasColor = true;
    return this;
}
Badge* Badge::WithSize(UiSize s) {
    size = s;
    return this;
}
Badge* Badge::Child(El* c) {
    child = c;
    return this;
}

El* Badge::IntoEl() {
    const Theme& th = ThemeNow();
    bool visible = kind != BadgeKind::Number || count > 0;
    El* root = Div(a);
    if (child) {
        root->Child(child);
    }
    if (!visible) {
        return root;
    }
    float box = 16;
    float font = 10;
    if (size == UiSize::Large) {
        box = 24;
        font = 14;
    } else if (size == UiSize::Small || size == UiSize::XSmall) {
        box = 10;
        font = 8;
    }
    Rgba bg = hasColor ? color : th.danger;
    El* mark = Div(a)
                   ->Absolute()
                   ->Top(-box * 0.35f)
                   ->Right(-box * 0.35f)
                   ->Bg(bg)
                   ->ItemsCenter()
                   ->JustifyCenter();
    if (kind == BadgeKind::Dot) {
        mark->W(8)->H(8)->Radius(4);
    } else if (kind == BadgeKind::Icon) {
        mark->W(box)
            ->H(box)
            ->Radius(box * 0.5f)
            ->Child(IconEl(a, icon, box * 0.6f)->Fg(th.dangerFg));
    } else {
        int shown = count > max ? max : count;
        Str txt = count > max ? StrDup(a, fmt("%d+", shown))
                              : StrDup(a, fmt("%d", shown));
        mark->MinW(box)
            ->H(box)
            ->PadX(4)
            ->Radius(box * 0.5f)
            ->Child(TextEl(a, txt)->Font(font)->Fg(th.dangerFg));
    }
    root->Child(mark);
    return root;
}

} // namespace component
} // namespace gpui
