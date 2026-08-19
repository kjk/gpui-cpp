#include "ui/select.h"
#include "ui/button.h"

namespace gpui {

namespace component {

Select* Select::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    Select* s = ArenaNew<Select>(a);
    s->a = a;
    s->cx = cx;
    s->id = id;
    return s;
}
Select* Select::Option(Str s) {
    if (n < 24) {
        options[n++] = s;
    }
    return this;
}
Select* Select::Options(const char* const* items, int count) {
    for (int i = 0; i < count; i++) {
        Option(Str(items[i]));
    }
    return this;
}
Select* Select::Selected(int i) {
    selected = i;
    return this;
}
Select* Select::Placeholder(Str s) {
    placeholder = s;
    return this;
}
Select* Select::TitlePrefix(Str s) {
    titlePrefix = s;
    return this;
}
Select* Select::Empty(Str s) {
    empty = s;
    return this;
}
Select* Select::W(float v) {
    width = v;
    return this;
}
Select* Select::MenuWidth(float v) {
    menuWidth = v;
    return this;
}
Select* Select::MenuMaxH(float v) {
    menuMaxH = v;
    return this;
}
Select* Select::WithSize(UiSize s) {
    size = s;
    return this;
}
Select* Select::Icon(IconName i) {
    icon = i;
    return this;
}
Select* Select::Disabled(bool v) {
    disabled = v;
    return this;
}
Select* Select::Cleanable(bool v) {
    cleanable = v;
    return this;
}
Select* Select::Appearance(bool v) {
    appearance = v;
    return this;
}
Select* Select::Open(bool v) {
    open = v;
    return this;
}
Select* Select::OnChange(Listener fn) {
    onChange = fn;
    return this;
}
Select* Select::OnToggle(Listener fn) {
    onToggle = fn;
    return this;
}
Select* Select::OnClear(Listener fn) {
    onClear = fn;
    return this;
}

El* Select::IntoEl() {
    const Theme& th = cx->theme();
    // input_size / input_text_size, by size.
    float h = 32, padX = 10, font = 14, caret = 16;
    if (size == UiSize::Large) {
        h = 44;
        padX = 12;
        font = 16;
    } else if (size == UiSize::Small) {
        h = 24;
        padX = 8;
        caret = 14;
    } else if (size == UiSize::XSmall) {
        h = 20;
        padX = 4;
        font = 12;
        caret = 12;
    }
    bool hasValue = selected >= 0 && selected < n;
    Str title = hasValue
                    ? options[selected]
                    : (placeholder.s ? placeholder : StrL("Please select"));
    El* trigger = Div(a)
                      ->FlexRow()
                      ->W(width)
                      ->H(h)
                      ->PadX(padX)
                      ->Gap(4)
                      ->ItemsCenter()
                      ->JustifyBetween();
    if (appearance) {
        trigger->Radius(th.radius)
            ->Bg(disabled ? th.muted : th.inputBg)
            ->Border(1, open ? th.ring : th.inputBorder);
    }
    Rgba fg = disabled ? th.mutedFg : th.foreground;
    if (hasValue && titlePrefix.s) {
        title = StrDup(a, fmt("%s%s", titlePrefix, title));
    }
    trigger
        ->Child(TextEl(a, title)->Font(font)->Fg(hasValue ? fg : th.mutedFg));
    if (cleanable && hasValue && !disabled) {
        trigger->Child(Button::New(cx, StrDup(a, fmt("%s-clean", id)))
                           ->Text()
                           ->WithSize(UiSize::XSmall)
                           ->Icon(IconName::X)
                           ->OnClick(onClear)
                           ->IntoEl());
    } else if (icon != IconName::None) {
        // A custom icon replaces the caret, at xsmall.
        trigger->Child(IconEl(a, icon, 12)->Fg(th.mutedFg));
    } else {
        trigger->Child(IconEl(a, IconName::ChevronDown, caret)->Fg(th.mutedFg));
    }
    if (!disabled) {
        BindClick(trigger, id, onToggle);
    }

    El* menu = nullptr;
    if (open && !disabled) {
        menu = Div(a)
                   ->FlexCol()
                   ->W(menuWidth > 0 ? menuWidth : width)
                   ->Pad(4)
                   ->Gap(1)
                   ->Radius(th.radiusLg)
                   ->Border(1, th.border)
                   ->Bg(th.background);
        if (menuMaxH > 0) {
            menu->H(menuMaxH)->ClipY();
        }
        if (n == 0) {
            // The story renders its own empty state; ours is the same shape.
            menu->Child(
                Div(a)->H(96)->W(kFill)->ItemsCenter()->JustifyCenter()->Child(
                    TextEl(a, empty.s ? empty : StrL("No Data"))
                        ->Font(font)
                        ->Fg(th.mutedFg)));
        }
        for (int i = 0; i < n; i++) {
            El* row = Div(a)
                          ->FlexRow()
                          ->W(kFill)
                          ->H(28)
                          ->PadX(8)
                          ->Gap(8)
                          ->ItemsCenter()
                          ->JustifyBetween()
                          ->Radius(th.radius)
                          ->HoverBg(th.accent);
            row->Child(TextEl(a, options[i])->Font(font)->Fg(th.foreground));
            if (i == selected) {
                row->Child(IconEl(a, IconName::Check, 14)->Fg(th.foreground));
            }
            if (onChange.IsValid()) {
                BindClick(row, StrDup(a, fmt("%s-opt%d", id, i)),
                          ListenerArg(onChange, i));
            }
            menu->Child(row);
        }
    }
    El* root = gpui::Select::New(cx, id)->W(width)->Child(trigger);
    return Popup::New(cx, StrDup(a, fmt("%s-popup", id)), root)
        ->Content(menu)
        ->IntoEl();
}

} // namespace component
} // namespace gpui
