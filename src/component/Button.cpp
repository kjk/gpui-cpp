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
    onClick = l;
    return this;
}

El* Button::IntoEl() {
    const Theme& th = cx->theme();
    // The default variant is the background with an input-colored border, not
    // a secondary fill: crates/ui falls `button` back to the theme background
    // in light mode, and hovers toward the input color.
    Rgba bg = th.background, fg = th.foreground,
         hover = RgbaOpacity(th.inputBorder, 0.5f), bd = th.inputBorder;
    // The status variants are washes, not fills: crates/ui's button_danger and
    // friends are the status color mixed 20% toward transparent, with the
    // color itself as the text and the same wash as the border. Outlined, the
    // wash drops to 10% and the border goes to 60%.
    Rgba accent = {};
    bool hasAccent = false;
    switch (variant) {
        case ButtonVariant::Secondary:
            bg = th.secondary;
            fg = th.secondaryFg;
            hover = th.secondaryHover;
            bd = th.border;
            break;
        case ButtonVariant::Primary:
            bg = th.primary;
            fg = th.primaryFg;
            hover = RgbaMix(th.primary, th.foreground, 0.85f);
            bd = th.primary;
            break;
        case ButtonVariant::Danger:
            accent = th.danger;
            hasAccent = true;
            break;
        case ButtonVariant::Success:
            accent = th.success;
            hasAccent = true;
            break;
        case ButtonVariant::Warning:
            accent = th.warning;
            hasAccent = true;
            break;
        case ButtonVariant::Info:
            accent = th.info;
            hasAccent = true;
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
    if (hasAccent) {
        bg = RgbaOpacity(accent, 0.2f);
        fg = accent;
        hover = RgbaOpacity(accent, 0.3f);
        bd = bg;
    }
    if (hasCustom) {
        fg = custom;
        bd = custom;
        bg = outline ? th.background : RgbaOpacity(custom, 0.12f);
        hover = RgbaOpacity(custom, 0.2f);
    }
    if (outline && !hasCustom) {
        if (hasAccent) {
            bg = RgbaOpacity(accent, 0.1f);
            bd = RgbaOpacity(accent, 0.6f);
            hover = RgbaOpacity(accent, 0.2f);
        } else if (variant == ButtonVariant::Primary) {
            bg = RgbaOpacity(th.primary, 0.1f);
            fg = th.primary;
            hover = RgbaOpacity(th.primary, 0.2f);
        } else {
            bg = th.background;
            hover = th.muted;
        }
    }
    if (selected) {
        bg = th.secondaryActive;
        hover = th.secondaryHover;
    }
    if (disabled) {
        fg = th.mutedFg;
    }
    // crates/ui/src/button: h_5/px_1, h_6/px_2, h_8/px_2p5, h_8/px_3, with a
    // tighter px when compact. Buttons do not use the generic control height.
    float h = 32.f;
    float padX = compact ? 8.f : 10.f;
    if (size == UiSize::XSmall) {
        h = 20.f;
        padX = 4.f;
    } else if (size == UiSize::Small) {
        h = 24.f;
        padX = compact ? 6.f : 8.f;
    } else if (size == UiSize::Large) {
        padX = compact ? 8.f : 12.f;
    }
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
        // button_text_size: text_xs, text_sm, then text_base — a step larger
        // than the generic control font.
        float fontPx = size == UiSize::XSmall  ? 12.f
                       : size == UiSize::Small ? 14.f
                                               : 16.f;
        e->Child(TextEl(a, label)->Font(fontPx)->Fg(fg));
    }
    if (dropdown) {
        e->Child(IconEl(a, IconName::ChevronDown, 12)->Fg(fg));
    }
    return e;
}

} // namespace component
} // namespace gpui
