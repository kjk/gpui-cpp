#include "ui/status_bar.h"

namespace gpui {

namespace component {

StatusBar* StatusBar::New(Ctx* cx) {
    Arena* a = cx->a;
    StatusBar* s = ArenaNew<StatusBar>(a);
    s->a = a;
    s->cx = cx;
    return s;
}
StatusBar* StatusBar::Left(Str s) {
    left = s;
    hasLeft = true;
    return this;
}
StatusBar* StatusBar::Center(Str s) {
    center = s;
    hasCenter = true;
    return this;
}
StatusBar* StatusBar::Right(Str s) {
    right = s;
    hasRight = true;
    return this;
}

El* StatusBar::IntoEl() {
    const Theme& th = cx->theme();
    El* bar = Div(a)
                  ->FlexRow()
                  ->W(kFill)
                  ->H(28)
                  ->PadX(12)
                  ->ItemsCenter()
                  ->Bg(th.tokens.statusBar)
                  ->BorderT(1, th.border);
    // Left and right hold their edges; the center takes what is left, so it
    // sits at the start with only a right side, at the end with only a left
    // one, and centered when both are there.
    if (hasLeft) {
        bar->Child(TextEl(a, left)->Font(12)->Fg(th.mutedFg));
    }
    El* mid = Div(a)->FlexRow()->Flex1()->ItemsCenter();
    if (hasLeft && hasRight) {
        mid->JustifyCenter();
    } else if (hasLeft) {
        mid->JustifyEnd();
    }
    if (hasCenter) {
        mid->Child(TextEl(a, center)->Font(12)->Fg(th.mutedFg));
    }
    bar->Child(mid);
    if (hasRight) {
        bar->Child(TextEl(a, right)->Font(12)->Fg(th.mutedFg));
    }
    return bar;
}

} // namespace component
} // namespace gpui
