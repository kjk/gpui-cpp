#include "component/Alert.h"

namespace gpui {

namespace component {

Alert* Alert::New(Ctx* cx, Str id, Str message) {
    Arena* a = cx->a;
    Alert* al = ArenaNew<Alert>(a);
    al->a = a;
    al->cx = cx;
    al->id = id;
    al->message = message;
    return al;
}

Alert* Alert::Info(Ctx* cx, Str id, Str message) {
    Arena* a = cx->a;
    Alert* al = New(cx, id, message);
    al->variant = AlertVariant::Info;
    al->icon = IconName::Info;
    return al;
}
Alert* Alert::Success(Ctx* cx, Str id, Str message) {
    Arena* a = cx->a;
    Alert* al = New(cx, id, message);
    al->variant = AlertVariant::Success;
    al->icon = IconName::CircleCheck;
    return al;
}
Alert* Alert::Warning(Ctx* cx, Str id, Str message) {
    Arena* a = cx->a;
    Alert* al = New(cx, id, message);
    al->variant = AlertVariant::Warning;
    al->icon = IconName::TriangleAlert;
    return al;
}
Alert* Alert::Error(Ctx* cx, Str id, Str message) {
    Arena* a = cx->a;
    Alert* al = New(cx, id, message);
    al->variant = AlertVariant::Error;
    al->icon = IconName::CircleX;
    return al;
}

Alert* Alert::Title(Str s) {
    title = s;
    return this;
}
Alert* Alert::Icon(IconName n) {
    icon = n;
    return this;
}
Alert* Alert::Content(El* e) {
    content = e;
    return this;
}
Alert* Alert::Banner() {
    banner = true;
    return this;
}
Alert* Alert::Visible(bool v) {
    visible = v;
    return this;
}
Alert* Alert::OnClose(Listener fn) {
    onClose = fn;
    return this;
}
Alert* Alert::WithSize(UiSize s) {
    size = s;
    return this;
}

El* Alert::IntoEl() {
    if (!visible) {
        return Div(a);
    }
    const Theme& th = cx->theme();
    Rgba fg = th.foreground, bg = th.background, bd = th.border;
    switch (variant) {
        case AlertVariant::Info:
            fg = th.info;
            bg = RgbaOpacity(th.info, 0.04f);
            bd = RgbaOpacity(th.info, 0.3f);
            break;
        case AlertVariant::Success:
            fg = th.success;
            bg = RgbaOpacity(th.success, 0.04f);
            bd = RgbaOpacity(th.success, 0.3f);
            break;
        case AlertVariant::Warning:
            fg = th.warning;
            bg = RgbaOpacity(th.warning, 0.04f);
            bd = RgbaOpacity(th.warning, 0.3f);
            break;
        case AlertVariant::Error:
            fg = th.danger;
            bg = RgbaOpacity(th.danger, 0.04f);
            bd = RgbaOpacity(th.danger, 0.3f);
            break;
        default:
            break;
    }
    El* row = Div(a)
                  ->FlexRow()
                  ->Gap(8)
                  ->Pad(banner ? 8.f : 12.f)
                  ->ItemsStart()
                  ->Bg(bg);
    if (!banner) {
        row->Border(1, bd)->Radius(th.radius);
    }
    row->Child(IconEl(a, icon, 16)->Fg(fg)->Shrink0());
    El* col = Div(a)->FlexCol()->Gap(4)->Grow();
    if (title.s && !banner) {
        col->Child(TextEl(a, title)->Font(14)->Semibold()->Fg(fg));
    }
    if (content) {
        col->Child(content);
    } else {
        col->Child(TextEl(a, message)->Font(UiFontPx(size))->Fg(fg)->Wrap());
    }
    row->Child(col);
    if (onClose.IsValid()) {
        // p_0p5 + rounded, icon at max(size, Medium) in the alert's color.
        El* x = Div(a)
                    ->Pad(2)
                    ->Radius(th.radius)
                    ->ItemsCenter()
                    ->JustifyCenter()
                    ->Shrink0()
                    ->Child(IconEl(a, IconName::X, 16)->Fg(fg));
        BindClick(x, StrL("alert-close"), onClose);
        row->Child(x);
    }
    return row;
}

} // namespace component
} // namespace gpui
