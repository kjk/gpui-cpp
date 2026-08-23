#include "ui/button.h"
#include "ui/menu.h"

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

Button* Button::IconColor(Rgba c) {
    hasIconColor = true;
    iconColor = c;
    return this;
}
Button* Button::IconRight(IconName n) {
    iconRight = n;
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
Button* Button::JustifyStart(bool v) {
    justifyStart = v;
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

Button* Button::SelectedStyle(const StateStyle& s) {
    selectedStyle = s;
    return this;
}

Button* Button::DisabledStyle(const StateStyle& s) {
    disabledStyle = s;
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
Button* Button::Size(float px) {
    sizePx = px;
    return this;
}
Button* Button::LoadingIcon(IconName n) {
    loadingIcon = n;
    return this;
}
Button* Button::TabIndex(int v) {
    tabIndex = v;
    return this;
}
Button* Button::TabStop(bool v) {
    tabStop = v;
    return this;
}
Button* Button::FocusRing(bool v) {
    focusRing = v;
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
    // bg and hover are Backgrounds, not colours: a theme may spell either as
    // a gradient, and a button is where most of them land.
    Background bg = th.tokens.background,
               hover = RgbaOpacity(th.inputBorder, 0.5f);
    Rgba fg = th.foreground, bd = th.inputBorder;
    // The status variants are washes, not fills: crates/ui's button_danger and
    // friends are the status color mixed 20% toward transparent, with the
    // color itself as the text and the same wash as the border. Outlined, the
    // wash drops to 10% and the border goes to 60%.
    // The status variants' accent is a fill, not a colour: schema.rs lets
    // `danger.background` and friends be gradients, and the wash a button
    // paints is that fill faded rather than a flat colour faded.
    Background accent = {};
    bool hasAccent = false;
    switch (variant) {
        case ButtonVariant::Secondary:
            bg = th.tokens.secondary;
            fg = th.secondaryFg;
            hover = th.secondaryHover;
            bd = th.border;
            break;
        case ButtonVariant::Primary:
            bg = th.tokens.primary;
            fg = th.primaryFg;
            hover = RgbaMix(th.primary, th.foreground, 0.85f);
            bd = th.primary;
            break;
        case ButtonVariant::Danger:
            accent = th.tokens.danger;
            hasAccent = true;
            break;
        case ButtonVariant::Success:
            accent = th.tokens.success;
            hasAccent = true;
            break;
        case ButtonVariant::Warning:
            accent = th.tokens.warning;
            hasAccent = true;
            break;
        case ButtonVariant::Info:
            accent = th.tokens.info;
            hasAccent = true;
            break;
        case ButtonVariant::Ghost:
        case ButtonVariant::Text:
            bg = Rgba8(0, 0, 0, 0);
            fg = th.foreground;
            hover = th.tokens.muted;
            bd = Rgba8(0, 0, 0, 0);
            break;
        case ButtonVariant::Link:
            bg = Rgba8(0, 0, 0, 0);
            fg = th.blue;
            hover = th.tokens.muted;
            bd = Rgba8(0, 0, 0, 0);
            break;
        default:
            break;
    }
    if (hasAccent) {
        bg = BackgroundOpacity(accent, 0.2f);
        fg = accent.color;
        hover = BackgroundOpacity(accent, 0.3f);
        bd = bg.color;
    }
    if (hasCustom) {
        fg = custom;
        bd = custom;
        bg = outline ? th.background : RgbaOpacity(custom, 0.12f);
        hover = RgbaOpacity(custom, 0.2f);
    }
    if (outline && !hasCustom) {
        if (hasAccent) {
            bg = BackgroundOpacity(accent, 0.1f);
            bd = RgbaOpacity(accent.color, 0.6f);
            hover = BackgroundOpacity(accent, 0.2f);
        } else if (variant == ButtonVariant::Primary) {
            bg = BackgroundOpacity(th.tokens.primary, 0.1f);
            fg = th.primary;
            hover = BackgroundOpacity(th.tokens.primary, 0.2f);
        } else {
            bg = th.tokens.background;
            hover = th.tokens.muted;
        }
    }
    if (selected) {
        bg = th.secondaryActive;
        hover = th.secondaryHover;
    }
    if (disabled) {
        // ButtonVariant::disabled. The foreground is muted at half — which is
        // the whole of what the port had — and the background and border go
        // with it: the variant's own fill at 0.15 (Secondary's at 1.5, which
        // is Rust's number and saturates), Default on the input surface at
        // half, and Ghost, Link and Text disabled to nothing at all.
        // Outlined, both are the normal outline pair at half.
        fg = RgbaOpacity(th.mutedFg, 0.5f);
        if (outline) {
            bg = BackgroundOpacity(bg, 0.5f);
            bd = RgbaOpacity(bd, 0.5f);
        } else if (hasCustom) {
            bg = RgbaOpacity(custom, 0.15f);
            bd = RgbaOpacity(custom, 0.15f);
        } else if (hasAccent) {
            bg = BackgroundOpacity(accent, 0.15f);
            bd = RgbaOpacity(accent.color, 0.15f);
        } else {
            switch (variant) {
                case ButtonVariant::Ghost:
                case ButtonVariant::Link:
                case ButtonVariant::Text:
                    bg = Rgba8(0, 0, 0, 0);
                    bd = Rgba8(0, 0, 0, 0);
                    break;
                case ButtonVariant::Primary:
                    bg = BackgroundOpacity(th.tokens.primary, 0.15f);
                    bd = RgbaOpacity(th.primary, 0.15f);
                    break;
                case ButtonVariant::Secondary:
                    bg = BackgroundOpacity(th.tokens.secondary, 1.f);
                    bd = th.secondary;
                    break;
                default:
                    bg = BackgroundOpacity(th.inputBg, 0.5f);
                    bd = RgbaOpacity(th.inputBorder, 0.5f);
                    break;
            }
        }
    }
    // state_style.rs resolve_style: whatever the caller asked for goes on last
    // and only for the fields it named — the value state, then disabled.
    const StateStyle* active[2] = {selected ? &selectedStyle : nullptr,
                                   disabled ? &disabledStyle : nullptr};
    StateStyle resolved = StateStyleResolve(StateStyle{}, active, 2);
    float borderW = 1;
    if (resolved.Has(StateFieldBg)) {
        bg = resolved.style.bg;
    }
    if (resolved.Has(StateFieldFg)) {
        fg = resolved.style.color;
    }
    if (resolved.Has(StateFieldBorder)) {
        borderW = resolved.style.border;
        bd = resolved.style.borderColor;
    }
    if (resolved.Has(StateFieldHoverBg)) {
        hover = resolved.style.hoverBg;
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
    // button.rs: `label.is_none() && children.is_empty()` is an Icon Button —
    // a square of the size's own side and no padding at all, rather than the
    // h/px pair a labelled button takes. `.icon()` is not a child in Rust, so
    // an icon alone still lands here; `extra` is what a `.child()` is here.
    bool iconOnly = !label.s && !extra;
    if (iconOnly) {
        h = size == UiSize::XSmall ? 20.f : size == UiSize::Small ? 24.f : 32.f;
        padX = 0;
    }
    if (variant == ButtonVariant::Text || variant == ButtonVariant::Link) {
        padX = 0;
        h = 0;
        iconOnly = false;
    }
    // Size::Size(px): a square of that size, with no room for a label.
    if (sizePx > 0) {
        h = sizePx;
        padX = 0;
    }
    // button.rs: gap_1 at the two small sizes, gap_2 above them.
    float gap = (size == UiSize::XSmall || size == UiSize::Small) ? 4.f : 8.f;
    // `icon_size`: the button's own size, and three quarters of it when the
    // caller gave a pixel size. Icon::with_size then resolves it —
    // size_3 / size_3p5 / size_4 / size_6.
    float iconPx = size == UiSize::XSmall  ? 12.f
                   : size == UiSize::Small ? 14.f
                   : size == UiSize::Large ? 24.f
                                           : 16.f;
    if (sizePx > 0) {
        iconPx = sizePx * 0.75f;
    }
    // The unstyled Button takes `disabled` here, and a click id of its own is
    // not one: passing one made every enabled button non-focusable and gave
    // disabled ones a focus handle, which is the opposite of both.
    bool interactive = !(disabled || loading);
    El* e = gpui::Button::New(cx, id, disabled)
                ->TabIndex(tabIndex)
                ->TabStop(tabStop)
                ->FocusRing(focusRing)
                ->H(h > 0 ? h : kAuto)
                ->PadX(padX)
                ->ItemsCenter()
                ->JustifyCenter()
                ->Gap(gap)
                ->Radius(resolved.Has(StateFieldRadius) ? resolved.style.radius
                                                        : th.radius);
    if (justifyStart) {
        e->JustifyStart();
    }
    if (sizePx > 0) {
        e->W(sizePx);
    } else if (iconOnly) {
        e->W(h);
    }
    if (bd.a) {
        if (joined) {
            // A joined child draws only the edges the group left it, and
            // takes its rounding from the group's own clip.
            e->Radius(0);
            if (edgeT) {
                e->BorderT(borderW, bd);
            }
            if (edgeB) {
                e->BorderB(borderW, bd);
            }
            if (edgeL) {
                e->BorderL(borderW, bd);
            }
            if (edgeR) {
                e->BorderR(borderW, bd);
            }
        } else {
            e->Border(borderW, bd);
        }
    }
    if (bg.gradient || bg.color.a) {
        e->Bg(bg);
    }
    if (!disabled) {
        e->HoverBg(hover);
        if (onClick.IsValid()) {
            e->OnClick(onClick);
        }
    }
    // button.rs: `cursor_default`, and the hand only for the two variants that
    // look like a link rather than a button. A ghost button is still a button,
    // so it keeps the arrow.
    if (interactive &&
        (variant == ButtonVariant::Link || variant == ButtonVariant::Text)) {
        e->Cursor(CursorKind::Pointer);
    }
    if (tooltip.s) {
        e->Tip(tooltip);
    }
    // button.rs fades the whole button while it loads rather than dimming its
    // colours one by one, and says why: Ghost, Link and Text have no
    // background for an alpha to show up on.
    if (loading && !disabled) {
        e->Opacity(0.8f);
    }
    if (extra) {
        e->Child(extra);
    } else if (loading) {
        e->Child(IconEl(a, loadingIcon, iconPx)->Fg(fg));
    } else if (icon != IconName::None) {
        El* ic = IconEl(a, icon, iconPx)->Fg(hasIconColor ? iconColor : fg);
        e->Child(ic);
    }
    if (label.s) {
        // button_text_size: text_xs, text_sm, then text_base — a step larger
        // than the generic control font.
        float fontPx = size == UiSize::XSmall  ? 12.f
                       : size == UiSize::Small ? 14.f
                                               : 16.f;
        El* text = TextEl(a, label)->Font(fontPx)->Fg(fg);
        // ButtonVariant::underline: only the link looks like a link.
        if (variant == ButtonVariant::Link) {
            text->Underline();
        }
        e->Child(text);
    }
    if (iconRight != IconName::None) {
        e->Child(IconEl(a, iconRight, iconPx)->Fg(fg));
    }
    if (dropdown) {
        // dropdown_caret adds the caret and nothing else: a DropdownButton's
        // seam is the border between its two buttons, not a rule inside one.
        // Caret::new(size): xs and sm keep their own icon size, everything
        // else — Large included — takes the medium one, at three quarters of
        // the button's own ink.
        float caretPx = size == UiSize::XSmall  ? 12.f
                        : size == UiSize::Small ? 14.f
                                                : 16.f;
        e->Child(IconEl(a, IconName::ChevronDown, caretPx)
                     ->Fg(RgbaOpacity(fg, 0.75f)));
    }
    return e;
}

DropdownButton* DropdownButton::New(Ctx* cx, Str id) {
    DropdownButton* d = ArenaNew<DropdownButton>(cx->a);
    d->a = cx->a;
    d->cx = cx;
    d->id = id;
    return d;
}
DropdownButton* DropdownButton::Button_(Button* b) {
    button = b;
    return this;
}
DropdownButton* DropdownButton::Menu(PopupMenu* m) {
    menu = m;
    return this;
}
DropdownButton* DropdownButton::Selected(bool v) {
    selected = v;
    return this;
}
DropdownButton* DropdownButton::Disabled(bool v) {
    disabled = v;
    return this;
}
DropdownButton* DropdownButton::Compact() {
    compact = true;
    return this;
}
DropdownButton* DropdownButton::Outline() {
    outline = true;
    return this;
}
DropdownButton* DropdownButton::Loading(bool v) {
    loading = v;
    return this;
}
DropdownButton* DropdownButton::WithVariant(ButtonVariant v) {
    hasVariant = true;
    variant = v;
    return this;
}
DropdownButton* DropdownButton::WithSize(UiSize s) {
    size = s;
    return this;
}
DropdownButton* DropdownButton::Tooltip(Str s) {
    tooltip = s;
    return this;
}

// The two buttons share the props and the seam between them. A ghost button
// that is not selected keeps both ends rounded — there is no filled block for
// a square corner to sit against — so the pair is not joined at all.
static void DropdownApply(Button* b, const DropdownButton& d) {
    b->Loading(d.loading)
        ->Selected(d.selected)
        ->Disabled(d.disabled || d.loading);
    if (d.compact) {
        b->Compact();
    }
    if (d.outline) {
        b->Outline();
    }
    b->WithSize(d.size);
    if (d.hasVariant) {
        b->variant = d.variant;
    }
}

El* DropdownButton::IntoEl() {
    const Theme& th = cx->theme();
    if (!button) {
        return Div(a);
    }
    bool rounded = variant == ButtonVariant::Ghost && !selected;
    El* row = Div(a)->Id(id)->FlexRow()->ItemsCenter();
    if (!rounded) {
        // Joined: the two ends are rounded by the wrapper and the seam is one
        // border, the way the Corners/Edges pair asks for.
        row->Radius(th.radius)->ClipX()->ClipY();
    }
    DropdownApply(button, *this);
    if (!rounded) {
        button->joined = true;
    }
    row->Child(button->IntoEl());

    Button* caret = Button::New(cx, StrDup(a, fmt("%s-popup", id)))
                        ->DropdownCaret();
    DropdownApply(caret, *this);
    caret->Loading(false);
    if (!rounded) {
        caret->joined = true;
        caret->edgeL = false;
    }
    El* trigger = caret->IntoEl();
    if (menu && !(disabled || loading)) {
        row->Child(DropdownMenu::New(cx, StrDup(a, fmt("%s-menu", id)))
                       ->Trigger(trigger)
                       ->Menu(menu)
                       ->AnchorRight(anchorRight)
                       ->IntoEl());
    } else {
        row->Child(trigger);
    }
    if (tooltip.s) {
        row->Tip(tooltip);
    }
    return row;
}

ButtonGroup* ButtonGroup::New(Ctx* cx, Str id) {
    ButtonGroup* g = ArenaNew<ButtonGroup>(cx->a);
    g->a = cx->a;
    g->cx = cx;
    g->id = id;
    return g;
}
ButtonGroup* ButtonGroup::Child(Button* b) {
    if (b && n < 8) {
        // child(): the group's `disabled` is pushed down as the child is
        // added, which is why the order of the two calls matters in Rust.
        b->Disabled(b->disabled || disabled);
        children[n++] = b;
    }
    return this;
}
ButtonGroup* ButtonGroup::Multiple(bool v) {
    multiple = v;
    return this;
}
ButtonGroup* ButtonGroup::Disabled(bool v) {
    disabled = v;
    return this;
}
ButtonGroup* ButtonGroup::Vertical(bool v) {
    vertical = v;
    return this;
}
ButtonGroup* ButtonGroup::Compact() {
    compact = true;
    return this;
}
ButtonGroup* ButtonGroup::Outline() {
    outline = true;
    return this;
}
ButtonGroup* ButtonGroup::WithVariant(ButtonVariant v) {
    hasVariant = true;
    variant = v;
    return this;
}
ButtonGroup* ButtonGroup::WithSize(UiSize s) {
    hasSize = true;
    size = s;
    return this;
}
ButtonGroup* ButtonGroup::OnClick(Listener l) {
    onClick = l;
    return this;
}

El* ButtonGroup::IntoEl() {
    const Theme& th = cx->theme();
    // The selection the group would report, which each child's click turns
    // into: its own index toggled in, or replacing the lot when single.
    intptr_t selected = 0;
    for (int i = 0; i < n; i++) {
        if (children[i]->selected) {
            selected |= (intptr_t)1 << i;
        }
    }
    El* box = Div(a)->Id(id);
    if (vertical) {
        // Rust's column stretches its children to the widest of them, because
        // taffy's default align_items is Stretch. Layout here only stretches
        // to a cross size the parent already has, and a group shrink-wraps,
        // so a vertical group's buttons stay as wide as their own labels.
        box->FlexCol()->JustifyCenter();
    } else {
        box->FlexRow()->ItemsCenter();
    }
    // The ends are rounded by the group, so a child never has to be.
    box->Radius(th.radius)->ClipX()->ClipY();
    for (int i = 0; i < n; i++) {
        Button* b = children[i];
        if (hasSize) {
            b->WithSize(size);
        }
        if (hasVariant) {
            b->variant = variant;
        }
        if (compact) {
            b->Compact();
        }
        if (outline) {
            b->Outline();
        }
        b->joined = n > 1;
        if (n > 1) {
            // First / middle / last: the seam between two children is drawn
            // once, by the one after it.
            b->edgeT = vertical ? (i == 0) : true;
            b->edgeL = vertical ? true : (i == 0);
            b->edgeB = true;
            b->edgeR = true;
        }
        if (onClick.IsValid() && !disabled) {
            intptr_t next = selected;
            intptr_t bit = (intptr_t)1 << i;
            if (multiple) {
                next ^= bit;
            } else {
                next = bit;
            }
            b->OnClick(ListenerArg(onClick, next));
        }
        box->Child(b->IntoEl());
    }
    return box;
}

} // namespace component
} // namespace gpui
