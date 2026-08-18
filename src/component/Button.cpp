#include "component/Button.h"

namespace gpui {

namespace component {

Button* Button::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    Button* b = ArenaNew<Button>(a);
    b->a = a;
    b->cx = cx;
    b->id = id;
    return b;
}

Button* Button::Label(Str s) {
    label = s;
    return this;
}
Button* Button::Icon(IconName n) {
    icon = n;
    return this;
}
Button* Button::Primary() {
    variant = ButtonVariant::Primary;
    return this;
}
Button* Button::Secondary() {
    variant = ButtonVariant::Secondary;
    return this;
}
Button* Button::Danger() {
    variant = ButtonVariant::Danger;
    return this;
}
Button* Button::Warning() {
    variant = ButtonVariant::Warning;
    return this;
}
Button* Button::Success() {
    variant = ButtonVariant::Success;
    return this;
}
Button* Button::Info() {
    variant = ButtonVariant::Info;
    return this;
}
Button* Button::Ghost() {
    variant = ButtonVariant::Ghost;
    return this;
}
Button* Button::Link() {
    variant = ButtonVariant::Link;
    return this;
}
Button* Button::Text() {
    variant = ButtonVariant::Text;
    return this;
}
Button* Button::Outline() {
    outline = true;
    return this;
}
Button* Button::Compact() {
    compact = true;
    return this;
}
Button* Button::Selected(bool v) {
    selected = v;
    return this;
}
Button* Button::DropdownCaret(bool v) {
    dropdown = v;
    return this;
}
Button* Button::Custom(Rgba c) {
    custom = c;
    hasCustom = true;
    return this;
}
Button* Button::Extra(El* e) {
    extra = e;
    return this;
}
Button* Button::Loading(bool v) {
    loading = v;
    return this;
}
Button* Button::Disabled(bool v) {
    disabled = v;
    return this;
}
Button* Button::WithSize(UiSize s) {
    size = s;
    return this;
}
Button* Button::Tooltip(Str s) {
    tooltip = s;
    return this;
}
Button* Button::OnClick(Listener l) {
    listener = l;
    return this;
}
Button* Button::OnClick(Func0 fn) {
    onClick = fn;
    return this;
}

El* Button::IntoEl() {
    const Theme& th = ThemeNow();
    Rgba bg = th.secondary, fg = th.secondaryFg, hover = th.secondaryHover,
         bd = th.border;
    switch (variant) {
        case ButtonVariant::Primary:
            bg = th.primary;
            fg = th.primaryFg;
            hover = RgbaMix(th.primary, th.foreground, 0.85f);
            bd = th.primary;
            break;
        case ButtonVariant::Danger:
            bg = th.danger;
            fg = th.dangerFg;
            hover = RgbaMix(th.danger, th.foreground, 0.85f);
            bd = th.danger;
            break;
        case ButtonVariant::Success:
            bg = th.success;
            fg = th.successFg;
            hover = RgbaMix(th.success, th.foreground, 0.85f);
            bd = th.success;
            break;
        case ButtonVariant::Warning:
            bg = th.warning;
            fg = th.warningFg;
            hover = RgbaMix(th.warning, th.foreground, 0.85f);
            bd = th.warning;
            break;
        case ButtonVariant::Info:
            bg = th.info;
            fg = th.infoFg;
            hover = RgbaMix(th.info, th.foreground, 0.85f);
            bd = th.info;
            break;
        case ButtonVariant::Ghost:
        case ButtonVariant::Text:
            bg = Rgba8(0, 0, 0, 0);
            fg = th.foreground;
            hover = th.muted;
            bd = Rgba8(0, 0, 0, 0);
            break;
        case ButtonVariant::Link:
            bg = Rgba8(0, 0, 0, 0);
            fg = th.blue;
            hover = th.muted;
            bd = Rgba8(0, 0, 0, 0);
            break;
        default:
            break;
    }
    if (hasCustom) {
        fg = custom;
        bd = custom;
        bg = outline ? th.background : RgbaOpacity(custom, 0.12f);
        hover = RgbaOpacity(custom, 0.2f);
    }
    if (outline && !hasCustom) {
        bg = th.background;
        hover = th.muted;
    }
    if (selected) {
        bg = th.secondaryActive;
        hover = th.secondaryHover;
    }
    if (disabled) {
        fg = th.mutedFg;
    }
    float h = UiSizePx(size);
    float padX = compact ? 8.f : 12.f;
    if (variant == ButtonVariant::Text || variant == ButtonVariant::Link) {
        padX = 0;
        h = 0;
    }
    El* e = gpui::Button::New(cx, id, disabled ? 0 : HashClickId(id))
                ->H(h > 0 ? h : kAuto)
                ->PadX(padX)
                ->ItemsCenter()
                ->JustifyCenter()
                ->Gap(6)
                ->Radius(th.radius);
    if (bd.a) {
        e->Border(1, bd);
    }
    if (bg.a) {
        e->Bg(bg);
    }
    if (!disabled) {
        e->HoverBg(hover);
        if (listener.IsValid()) {
            e->OnClick(listener);
        }
        if (onClick.IsValid()) {
            e->OnClick(onClick);
        }
    }
    if (tooltip.s) {
        e->Tip(tooltip);
    }
    if (extra) {
        e->Child(extra);
    } else if (loading) {
        e->Child(IconEl(a, IconName::Loader, 14)->Fg(fg));
    } else if (icon != IconName::None) {
        e->Child(IconEl(a, icon, 14)->Fg(fg));
    }
    if (label.s) {
        e->Child(TextEl(a, label)->Font(UiFontPx(size))->Fg(fg));
    }
    if (dropdown) {
        e->Child(IconEl(a, IconName::ChevronDown, 12)->Fg(fg));
    }
    return e;
}

} // namespace component
} // namespace gpui
