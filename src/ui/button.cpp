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
Button* Button::AccessibilityId(Str s) {
    accessibilityId = s;
    return this;
}
Button* Button::Role(AccessibilityRole value) {
    accessibilityRole = value;
    hasAccessibilityRole = true;
    return this;
}
Button* Button::Toggled(bool v) {
    accessibilityToggled = v;
    hasAccessibilityToggled = true;
    return this;
}
Button* Button::OnClick(Listener l) {
    onClick = l;
    return this;
}
Button* Button::OnClickAction(uint32_t action, intptr_t arg) {
    clickAction = action;
    clickActionArg = arg;
    return this;
}

El* Button::IntoEl() {
    const Theme& th = ThemeNow(cx->app);
    const Rgba clear = Rgba8(0, 0, 0, 0);
    const bool dark = ThemeGet(cx->app) == ThemeMode::Dark;
    // ButtonVariant::bg_color / hovered / active / border_color / text_color,
    // in button.rs. Every fill a variant paints is a *token* there —
    // `tokens.button_primary_hover` and not a mix computed at the call — so
    // that a theme file naming `button.primary.hover.background` is honoured.
    // The mixes this used to do are still the numbers, but they live in
    // `ThemeFillDerived` as the fallbacks schema.rs spells, which is the one
    // place they belong.
    //
    // bg, hover and press are Backgrounds, not colours: a theme may spell any
    // of them as a gradient, and a button is where most of them land.
    Background bg = th.tokens.button, hover = th.tokens.buttonHover,
               press = th.tokens.buttonActive;
    Rgba fg = th.buttonFg, bd = th.inputBorder;
    // hovered().fg / active().fg only ever differ from normal().fg for Link
    // and Text, the two that paint no fill: what the pointer moves on them is
    // the ink. Everything else answers the pointer with its background.
    Rgba fgHover = {};
    bool hasFgHover = false;
    // The status variants keep their accent around because `outline` re-derives
    // its three fills from it — `outline_background` is written against
    // `tokens.danger` and friends, not against the `button_danger` family.
    Background accent = {};
    bool hasAccent = false;
    switch (variant) {
        case ButtonVariant::Secondary:
            bg = th.tokens.buttonSecondary;
            fg = th.buttonSecondaryFg;
            hover = th.tokens.buttonSecondaryHover;
            press = th.tokens.buttonSecondaryActive;
            bd = th.border;
            break;
        case ButtonVariant::Primary:
            bg = th.tokens.buttonPrimary;
            fg = th.buttonPrimaryFg;
            hover = th.tokens.buttonPrimaryHover;
            press = th.tokens.buttonPrimaryActive;
            bd = th.primary;
            break;
        case ButtonVariant::Danger:
            accent = th.tokens.danger;
            hasAccent = true;
            bg = th.tokens.buttonDanger;
            fg = th.buttonDangerFg;
            hover = th.tokens.buttonDangerHover;
            press = th.tokens.buttonDangerActive;
            bd = th.buttonDanger;
            break;
        case ButtonVariant::Success:
            accent = th.tokens.success;
            hasAccent = true;
            bg = th.tokens.buttonSuccess;
            fg = th.buttonSuccessFg;
            hover = th.tokens.buttonSuccessHover;
            press = th.tokens.buttonSuccessActive;
            bd = th.buttonSuccess;
            break;
        case ButtonVariant::Warning:
            accent = th.tokens.warning;
            hasAccent = true;
            bg = th.tokens.buttonWarning;
            fg = th.buttonWarningFg;
            hover = th.tokens.buttonWarningHover;
            press = th.tokens.buttonWarningActive;
            bd = th.buttonWarning;
            break;
        case ButtonVariant::Info:
            accent = th.tokens.info;
            hasAccent = true;
            bg = th.tokens.buttonInfo;
            fg = th.buttonInfoFg;
            hover = th.tokens.buttonInfoHover;
            press = th.tokens.buttonInfoActive;
            bd = th.buttonInfo;
            break;
        case ButtonVariant::Ghost:
            // The one family with no token of its own: button.rs computes it
            // from `secondary`, lightened in dark and darkened in light, and
            // at 0.8 alpha in both. Twice as far for the pressed state.
            bg = clear;
            fg = th.secondaryFg;
            hover = RgbaOpacity(dark ? RgbaLighten(th.secondary, 0.1f)
                                     : RgbaDarken(th.secondary, 0.1f),
                                0.8f);
            press = RgbaOpacity(dark ? RgbaLighten(th.secondary, 0.2f)
                                     : RgbaDarken(th.secondary, 0.2f),
                                0.8f);
            bd = clear;
            break;
        case ButtonVariant::Text:
            // Link and Text paint no fill in any state — what changes under
            // the pointer is the text colour, which `HoverFg` carries.
            bg = clear;
            fg = RgbaOpacity(th.foreground, 0.9f);
            fgHover = th.foreground;
            hasFgHover = true;
            hover = clear;
            press = clear;
            bd = clear;
            break;
        case ButtonVariant::Link:
            bg = clear;
            fg = th.link;
            fgHover = th.linkHover;
            hasFgHover = true;
            hover = clear;
            press = clear;
            bd = clear;
            break;
        default:
            break;
    }
    if (hasCustom) {
        fg = custom;
        bd = custom;
        bg = outline ? th.background : RgbaOpacity(custom, 0.12f);
        hover = RgbaOpacity(custom, 0.2f);
        press = RgbaOpacity(custom, 0.3f);
    }
    if (outline && !hasCustom) {
        // outline_background(state): the semantic token at 0.1 / 0.2 / 0.4,
        // and Default on the input colour mixed toward transparent instead.
        if (hasAccent) {
            bg = BackgroundOpacity(accent, 0.1f);
            bd = RgbaOpacity(accent.color, 0.6f);
            hover = BackgroundOpacity(accent, 0.2f);
            press = BackgroundOpacity(accent, 0.4f);
        } else if (variant == ButtonVariant::Primary) {
            bg = BackgroundOpacity(th.tokens.primary, 0.1f);
            fg = th.primary;
            hover = BackgroundOpacity(th.tokens.primaryHover, 0.2f);
            press = BackgroundOpacity(th.tokens.primaryActive, 0.4f);
        } else if (variant == ButtonVariant::Secondary) {
            bg = BackgroundOpacity(th.tokens.secondary, 0.1f);
            fg = th.secondaryFg;
            hover = BackgroundOpacity(th.tokens.secondaryHover, 0.2f);
            press = BackgroundOpacity(th.tokens.secondaryActive, 0.4f);
        } else if (variant == ButtonVariant::Ghost ||
                   variant == ButtonVariant::Link ||
                   variant == ButtonVariant::Text) {
            // Transparent in every state, outlined or not.
        } else {
            bg = th.inputBg;
            hover = RgbaMixOklab(th.inputBorder, clear, 0.5f);
            press = RgbaMixOklab(th.inputBorder, clear, 0.7f);
        }
    }
    if (selected) {
        // ButtonVariant::selected: the variant's *own* active fill, not a
        // secondary one for everybody. Ghost is the exception — it has no
        // button family, so it takes `secondary_active` straight — and Link
        // and Text stay transparent the way they are in every other state.
        switch (variant) {
            case ButtonVariant::Ghost:
                bg = th.tokens.secondaryActive;
                break;
            case ButtonVariant::Link:
            case ButtonVariant::Text:
                bg = clear;
                break;
            default:
                bg = press;
                break;
        }
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
    if (resolved.Has(StateFieldActiveBg)) {
        press = resolved.style.activeBg;
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
    // The unstyled Button takes the interaction gate here. Loading is inert
    // without taking disabled styling, so its visual state stays separate
    // while Base still removes its focus and activation behavior.
    bool interactive = !(disabled || loading);
    AccessibilityRole role =
        hasAccessibilityRole
            ? accessibilityRole
            : variant == ButtonVariant::Link ? AccessibilityRole::Link
                                             : AccessibilityRole::Button;
    El* e = gpui::Button::New(cx, id, !interactive)
                ->Role(role)
                ->AriaDisabled(!interactive)
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
    if (accessibilityId.s) {
        e->AccessibilityId(accessibilityId);
    }
    if (hasAccessibilityToggled) {
        e->AriaToggled(accessibilityToggled ? AccessibilityToggled::True
                                           : AccessibilityToggled::False);
    }
    if (label.s) {
        e->AriaLabel(label);
    } else if (tooltip.s) {
        // Icon-only buttons use their tooltip as the accessible name, the
        // same fallback `Button::accessibility_label` takes upstream.
        e->AriaLabel(tooltip);
    }
    if (justifyStart) {
        e->JustifyStart();
    }
    if (sizePx > 0) {
        e->W(sizePx);
    } else if (iconOnly) {
        e->W(h);
    } else if (compact && h > 0) {
        // button.rs: compact is `min_w_5` / `min_w_6` / `min_w_8` beside the
        // tighter px, so a labelled compact button is never narrower than it
        // is tall — which is what keeps a pagination page number square.
        e->MinW(h);
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
    if (interactive && onClick.IsValid()) {
        e->OnClick(onClick);
    }
    if (interactive && clickAction) {
        e->OnClickAction(clickAction, clickActionArg);
    }
    // The ink the label and the icons inherit, so the pointer can move it:
    // `text_color(normal_style.fg)` on the root, with `hover` and `active`
    // refining it. Only the caret names a colour of its own, since Rust
    // builds it from `normal_style.fg` and it does not follow the state.
    e->Fg(fg);
    // button.rs hangs the hover and the active style off
    // `when(!disabled && !selected)` and then `when(interactive)`: a selected
    // button shows its selected fill and nothing else, and a loading one
    // keeps its normal colours because it is not waiting for another click.
    if (interactive && !selected) {
        e->HoverBg(hover);
        e->ActiveBg(press);
        if (hasFgHover) {
            e->HoverFg(fgHover);
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
        e->Child(IconEl(a, loadingIcon, iconPx));
    } else if (icon != IconName::None) {
        El* ic = IconEl(a, icon, iconPx);
        if (hasIconColor) {
            ic->Fg(iconColor);
        }
        e->Child(ic);
    }
    if (label.s) {
        // button_text_size: text_xs, text_sm, then text_base — a step larger
        // than the generic control font.
        float fontPx = size == UiSize::XSmall  ? 12.f
                       : size == UiSize::Small ? 14.f
                                               : 16.f;
        // `line_height(relative(1.))` on the base button: with the inherited
        // line height the text box is taller than the glyphs, so the padding
        // no longer decides the control's height and a button cannot be sized
        // precisely. A label is one line and is cut rather than wrapped —
        // `min_w_0`, `whitespace_nowrap`, `truncate` — since a button that
        // grew a second line would push everything around it.
        El* text = TextEl(a, label)->Font(fontPx)->LineHeight(1.f)->Truncate();
        // ButtonVariant::underline: only the link looks like a link.
        if (variant == ButtonVariant::Link) {
            text->Underline();
        }
        e->Child(text);
    }
    if (iconRight != IconName::None) {
        e->Child(IconEl(a, iconRight, iconPx));
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
DropdownButton* DropdownButton::Outline() {
    outline = true;
    return this;
}
DropdownButton* DropdownButton::WithVariant(ButtonVariant v) {
    hasVariant = true;
    variant = v;
    return this;
}
DropdownButton* DropdownButton::WithSize(UiSize s) {
    hasSize = true;
    size = s;
    return this;
}

// The props the two halves share. An outer variant or size applies to both;
// when either is unset the inner button's own becomes the shared value, so a
// caller can style the split from either level. Nothing here is invented for
// the caret: `compact`, `loading` and the tooltip stay on the action button.
static ButtonVariant DropdownVariant(const DropdownButton& d) {
    if (d.hasVariant) {
        return d.variant;
    }
    return d.button ? d.button->variant : ButtonVariant::Default;
}
static UiSize DropdownSize(const DropdownButton& d) {
    if (d.hasSize) {
        return d.size;
    }
    return d.button ? d.button->size : UiSize(UiSize::Medium);
}

El* DropdownButton::IntoEl() {
    const Theme& th = ThemeNow(cx->app);
    ButtonVariant v = DropdownVariant(*this);
    UiSize sz = DropdownSize(*this);
    // An inner selected state is the split's, rather than being cleared by the
    // DropdownButton's own default.
    bool isSelected = selected || (button && button->selected);
    // A ghost split that is not selected keeps both ends rounded -- there is no
    // filled block for a square corner to sit against -- so the pair is not
    // joined at all.
    bool attached = !(v == ButtonVariant::Ghost && !isSelected);

    IdScope scope(cx, id);
    El* row = Div(a)->Id(id)->FlexRow()->ItemsCenter();
    if (attached) {
        // Joined: the two ends are rounded by the wrapper and the seam is one
        // border, the way the Corners/Edges pair asks for.
        row->Radius(th.radius)->ClipX()->ClipY();
    }

    if (button) {
        button->Selected(isSelected)
            ->Disabled(disabled || button->disabled)
            ->WithSize(sz);
        button->variant = v;
        if (outline) {
            button->Outline();
        }
        if (attached) {
            button->joined = true;
        }
        row->Child(button->IntoEl());
    }

    // The trigger renders on its own account rather than disappearing with the
    // action button, and a loading action button leaves it available: loading
    // is action-specific, `Disabled(true)` is what shuts both halves.
    if (menu) {
        Button* caret = Button::New(cx, StrL("popup"))
                            ->DropdownCaret()
                            ->Selected(isSelected)
                            ->Disabled(disabled)
                            ->WithSize(sz);
        caret->variant = v;
        if (outline) {
            caret->Outline();
        }
        if (attached) {
            caret->joined = true;
            caret->edgeL = false;
        }
        El* trigger = caret->IntoEl();
        if (disabled) {
            row->Child(trigger);
        } else {
            row->Child(DropdownMenu::New(cx, StrL("menu"))
                           ->Trigger(trigger)
                           ->Menu(menu)
                           ->AnchorRight(anchorRight)
                           ->IntoEl());
        }
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
    if (b) {
        // child(): the group's `disabled` is pushed down as the child is
        // added, which is why the order of the two calls matters in Rust.
        b->Disabled(b->disabled || disabled);
        children.Append(a, b);
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

// The group's builder and its selected-index scratch are frame-owned, while a
// child listener has to survive until input dispatch. Rust uses an Rc<Cell>
// shared by the rendered children and group; a keyed entity is the equivalent
// lifetime seam in this runtime.
struct ButtonGroupState {
    Vec<int> selected;
    bool multiple = false;
    Listener onClick;

    static void OnChildClick(ButtonGroupState* self, Ctx* cx,
                             const ClickEvent*, intptr_t childIndex) {
        Vec<int> next = self->selected;
        int at = -1;
        for (int i = 0; i < next.len; i++) {
            if (next[i] == (int)childIndex) {
                at = i;
                break;
            }
        }
        if (self->multiple) {
            if (at >= 0) {
                for (int i = at + 1; i < next.len; i++) {
                    next[i - 1] = next[i];
                }
                next.len--;
            } else {
                next.Append((int)childIndex);
            }
        } else {
            next.Clear();
            next.Append((int)childIndex);
        }

        ButtonGroupEvent ev{next.els, next.len};
        ListenerCall(cx->app, cx->win, self->onClick, &ev);
    }
};

El* ButtonGroup::IntoEl() {
    const Theme& th = ThemeNow(cx->app);
    Entity<ButtonGroupState> state;
    ButtonGroupState* stateValue = nullptr;
    if (onClick.IsValid() && !disabled) {
        state = ElementStateEntity<ButtonGroupState>(
            cx, id, StrL("gpui::component::ButtonGroupState"));
        stateValue = state.Get(cx->app);
        if (stateValue) {
            stateValue->selected.Clear();
            for (int i = 0; i < children.len; i++) {
                if (children[i]->selected) {
                    stateValue->selected.Append(i);
                }
            }
            stateValue->multiple = multiple;
            stateValue->onClick = onClick;
        }
    }
    El* box = gpui::ToggleGroup::New(
        cx, id, vertical ? Axis::Vertical : Axis::Horizontal);
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
    for (int i = 0; i < children.len; i++) {
        Button* b = children[i];
        // A button's selected presentation alone is not a toggle state, but
        // membership in ButtonGroup is: upstream stamps every group child.
        b->Toggled(b->selected);
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
        b->joined = children.len > 1;
        if (children.len > 1) {
            // First / middle / last: the seam between two children is drawn
            // once, by the one after it.
            b->edgeT = vertical ? (i == 0) : true;
            b->edgeL = vertical ? true : (i == 0);
            b->edgeB = true;
            b->edgeR = true;
        }
        if (stateValue) {
            // Installing the group callback replaces a child's callback, as
            // Button::on_click does in the Rust map that builds the group.
            b->OnClick(ListenTo(state, &ButtonGroupState::OnChildClick, i));
        }
        box->Child(b->IntoEl());
    }
    return box;
}

} // namespace component
} // namespace gpui
