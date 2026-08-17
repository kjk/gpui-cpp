#include "component/Tag.h"

namespace gpui {

namespace component {

Tag* Tag::New(Arena* a, Str text) {
    Tag* t = ArenaNew<Tag>(a);
    t->a = a;
    t->text = text;
    return t;
}

Tag* Tag::Primary() {
    variant = TagVariant::Primary;
    return this;
}
Tag* Tag::Secondary() {
    variant = TagVariant::Secondary;
    return this;
}
Tag* Tag::Danger() {
    variant = TagVariant::Danger;
    return this;
}
Tag* Tag::Success() {
    variant = TagVariant::Success;
    return this;
}
Tag* Tag::Warning() {
    variant = TagVariant::Warning;
    return this;
}
Tag* Tag::Info() {
    variant = TagVariant::Info;
    return this;
}
Tag* Tag::Outline() {
    outline = true;
    return this;
}
Tag* Tag::WithSize(UiSize s) {
    size = s;
    return this;
}
Tag* Tag::Radius(float v) {
    radius = v;
    return this;
}
Tag* Tag::Custom(Rgba bg, Rgba fg) {
    customBg = bg;
    customFg = fg;
    hasCustom = true;
    return this;
}

El* Tag::IntoEl() {
    const Theme& th = ThemeNow();
    Rgba bg = th.secondary, fg = th.secondaryFg, bd = th.border;
    switch (variant) {
        case TagVariant::Primary:
            bg = th.primary;
            fg = th.primaryFg;
            bd = th.primary;
            break;
        case TagVariant::Danger:
            bg = th.danger;
            fg = th.dangerFg;
            bd = th.danger;
            break;
        case TagVariant::Success:
            bg = th.success;
            fg = th.successFg;
            bd = th.success;
            break;
        case TagVariant::Warning:
            bg = th.warning;
            fg = th.warningFg;
            bd = th.warning;
            break;
        case TagVariant::Info:
            bg = th.info;
            fg = th.infoFg;
            bd = th.info;
            break;
        default:
            break;
    }
    if (hasCustom) {
        bg = customBg;
        fg = customFg;
        bd = customFg;
    }
    if (outline) {
        fg = bd;
        bg = th.background;
    }
    float r = radius >= 0 ? radius : th.radius * 0.5f;
    return Div(a)
        ->PadX(size == UiSize::Small ? 6.f : 8.f)
        ->PadY(2)
        ->Radius(r)
        ->Bg(bg)
        ->Border(1, bd)
        ->ItemsCenter()
        ->Child(TextEl(a, text)->Font(UiFontPx(size) - 2)->Fg(fg));
}

} // namespace component
} // namespace gpui
