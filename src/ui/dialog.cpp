#include "ui/dialog.h"
#include "base/motion.h"
#include "ui/button.h"

namespace gpui {

namespace component {

// dialog.rs: ANIMATION_DURATION, and the curve the panel comes down along.
// Rust names it inline as `cubic_bezier(0.32, 0.72, 0., 1.)`; an easing here
// is a function pointer, so the curve is a function.
static const float kDialogMotionMs = 250.f;

static float DialogEase(float t) {
    return CubicBezier(0.32f, 0.72f, 0.f, 1.f, t);
}

Dialog* Dialog::New(Ctx* cx) {
    Arena* a = cx->a;
    Dialog* d = ArenaNew<Dialog>(a);
    d->a = a;
    d->cx = cx;
    return d;
}
Dialog* Dialog::Title(Str s) {
    title = s;
    return this;
}
Dialog* Dialog::Description(Str s) {
    description = s;
    return this;
}
Dialog* Dialog::Open(bool v) {
    open = v;
    return this;
}
Dialog* Dialog::Body(El* e) {
    body = e;
    return this;
}
Dialog* Dialog::Surface(El* e) {
    surface = e;
    return this;
}
Dialog* Dialog::W(float px) {
    width = px;
    return this;
}
Dialog* Dialog::H(float px) {
    height = px;
    return this;
}
Dialog* Dialog::Overlay(bool v) {
    overlay = v;
    return this;
}
Dialog* Dialog::OverlayClosable(bool v) {
    overlayClosable = v;
    return this;
}
Dialog* Dialog::Keyboard(bool v) {
    keyboard = v;
    return this;
}
Dialog* Dialog::Layer(int ix) {
    layerIx = ix;
    return this;
}
Dialog* Dialog::Radius(float px) {
    radius = px;
    return this;
}
Dialog* Dialog::Bg(Background color) {
    background = color;
    hasBackground = true;
    return this;
}
Dialog* Dialog::Fg(Rgba color) {
    foreground = color;
    hasForeground = true;
    return this;
}
Dialog* Dialog::Icon(IconName n, Rgba color, float size) {
    icon = n;
    iconColor = color;
    hasIconColor = true;
    iconSize = size;
    return this;
}
Dialog* Dialog::HeaderCentered(bool v) {
    headerCentered = v;
    return this;
}
Dialog* Dialog::OkText(Str s) {
    okText = s;
    return this;
}
Dialog* Dialog::CancelText(Str s) {
    cancelText = s;
    return this;
}
Dialog* Dialog::OkVariant(ButtonVariant v, bool outline) {
    okVariant = v;
    okOutline = outline;
    return this;
}
Dialog* Dialog::ShowCancel(bool v) {
    showCancel = v;
    return this;
}
Dialog* Dialog::Confirm() {
    showCancel = true;
    return this;
}
Dialog* Dialog::CloseButton(bool v) {
    closeButton = v;
    return this;
}
Dialog* Dialog::Footer(El* e) {
    footer = e;
    return this;
}
Dialog* Dialog::FooterVertical(bool v) {
    footerVertical = v;
    return this;
}
Dialog* Dialog::FooterStretch(bool v) {
    footerStretch = v;
    return this;
}
Dialog* Dialog::FooterMuted(bool v) {
    footerMuted = v;
    return this;
}
Dialog* Dialog::FooterDivider(bool v) {
    footerDivider = v;
    return this;
}
Dialog* Dialog::OnClose(Listener fn) {
    onClose = fn;
    return this;
}
Dialog* Dialog::OnCancel(Listener fn) {
    onCancel = fn;
    return this;
}
Dialog* Dialog::OnOk(Listener fn) {
    onOk = fn;
    return this;
}

// DialogHeader: the icon, title and description, centered as a group once
// there is an icon above them.
El* Dialog::Header() {
    const Theme& th = cx->theme();
    El* head = Div(a)->FlexCol()->W(kFill)->Pad(16)->Gap(8);
    El* ic = nullptr;
    if (icon != IconName::None) {
        ic = IconEl(a, icon, iconSize)->Shrink0();
        if (hasIconColor) {
            ic->Fg(iconColor);
        }
    }
    if (headerCentered) {
        head->ItemsCenter();
        if (ic) {
            head->Child(ic);
        }
        ic = nullptr;
    }
    if (title.s && title.len > 0) {
        El* text = TextEl(a, title)->Font(16)->Semibold()->Fg(th.foreground);
        El* line = text;
        if (ic) {
            line = Div(a)->FlexRow()->Gap(8)->ItemsCenter()->Child(ic)->Child(
                text);
        }
        head->Child(DialogTitle::New(cx)->Child(line));
    } else if (ic) {
        head->Child(ic);
    }
    if (description.s && description.len > 0) {
        head->Child(DialogDescription::New(cx)->Child(TextEl(a, description)
                                                          ->Font(14)
                                                          ->Fg(th.mutedFg)
                                                          ->Wrap()
                                                          ->W(kFill)));
    }
    if (body) {
        head->Child(body);
    }
    return head;
}

// An id with the layer index on it, which is what makes two open dialogs two
// sets of controls rather than one shared set.
Str Dialog::LayerId(Str base) const {
    return StrDup(a, fmt("%s-%d", base, layerIx));
}

// DialogFooter: the action row, or whatever the caller put in its place.
El* Dialog::Actions() {
    const Theme& th = cx->theme();
    El* row = Div(a)->W(kFill)->Pad(16)->Gap(8);
    if (footerVertical) {
        row->FlexCol();
    } else {
        row->FlexRow()->JustifyEnd();
    }
    if (footerMuted) {
        row->Bg(th.tokens.muted);
    }
    if (footerDivider) {
        row->BorderT(1, th.border);
    }
    if (footer) {
        row->Child(footer);
        return row;
    }

    // Every id here carries the layer: GPUI scopes an ElementId by its
    // ancestors' and the panel is `.id(layer_ix)`, so two open dialogs give
    // their buttons and their close x distinct identities. Flat hashes here
    // do not, and two dialogs at once shared one hover state — pointing at
    // the top one's close x lit up the one behind it too.
    El* cancel = nullptr;
    if (showCancel) {
        cancel = Button::New(cx, LayerId(StrL("dialog-cancel")))
                     ->Label(cancelText.s ? cancelText : StrL("Cancel"))
                     ->Outline()
                     ->OnClick(onCancel.IsValid() ? onCancel : onClose)
                     ->IntoEl();
    }
    Button* okBtn = Button::New(cx, LayerId(StrL("dialog-ok")))
                        ->Label(okText.s ? okText : StrL("OK"))
                        ->OnClick(onOk);
    switch (okVariant) {
        case ButtonVariant::Danger:
            okBtn->Danger();
            break;
        case ButtonVariant::Default:
            break;
        default:
            okBtn->Primary();
            break;
    }
    if (okOutline) {
        okBtn->Outline();
    }
    El* ok = okBtn->IntoEl();

    // Stacked, the primary action leads; in a row it sits at the end.
    if (footerVertical) {
        row->Child(ok->W(kFill));
        if (cancel) {
            row->Child(cancel->W(kFill));
        }
        return row;
    }
    if (cancel) {
        row->Child(footerStretch ? cancel->Flex1() : cancel);
    }
    row->Child(footerStretch ? ok->Flex1() : ok);
    return row;
}

El* Dialog::IntoEl(WinSize size) {
    if (!open) {
        return Div(a);
    }
    const Theme& th = cx->theme();
    // The parts carry the padding, so a footer that tints or rules itself
    // reaches the panel's edges (AlertDialog::p_0 in the Rust story).
    El* panel = Div(a)
                    ->W(width)
                    ->FlexCol()
                    ->MinH(96)
                    ->Bg(hasBackground ? background : th.background)
                    ->Border(1, th.border)
                    ->Radius(radius > 0 ? radius : th.radiusLg)
                    ->ClipY();
    if (height > 0) {
        panel->H(height);
    }
    if (hasForeground) {
        panel->Fg(foreground);
    }
    if (surface) {
        panel->Child(surface);
    } else {
        panel->Child(Header());
        panel->Child(Actions());
    }
    if (closeButton) {
        El* x = Div(a)
                    ->Absolute()
                    ->Top(8)
                    ->Right(8)
                    ->W(24)
                    ->H(24)
                    ->ItemsCenter()
                    ->JustifyCenter()
                    ->Radius(th.radius)
                    ->HoverBg(th.secondaryHover)
                    ->Child(IconEl(a, IconName::X, 14)->Fg(th.mutedFg));
        BindClick(x, LayerId(StrL("dialog-close-x")), onClose);
        panel->Child(x);
    }
    // Fixed, not absolute: Rust hangs the dialog off the window Root, so it
    // covers and centers on the window rather than on whatever page element
    // happens to contain it.
    // "fade-in" and "slide-down": the whole layer fades in over a quarter of a
    // second while the panel comes down from the top edge.
    float delta = MotionAppear(
        cx, MotionId(StrL("dialog"), StrDup(a, fmt("%d", layerIx))),
        kDialogMotionMs, DialogEase);
    El* backdrop =
        DialogBackdrop::New(cx)->Fixed()->Top(0)->Left(0)->W(kFill)->H(kFill);
    if (overlay) {
        backdrop->Bg(th.tokens.overlay);
    }
    if (overlayClosable && onClose.IsValid()) {
        backdrop->OnClick(onClose)
            ->Click(HashClickId(LayerId(StrL("dialog-backdrop"))));
    }
    // DialogProps::margin_top: a tenth of the viewport down from the top,
    // not centered in it.
    El* popup = DialogPopup::New(cx)
                    ->Fixed()
                    ->Top(0)
                    ->Left(0)
                    ->W(kFill)
                    ->H(kFill)
                    ->FlexCol()
                    ->ItemsCenter()
                    ->PadT((size.dipH * 0.1f + layerIx * 16.f) * delta)
                    ->Child(panel);
    Str trap = StrDup(a, fmt("dialog-%d", layerIx));
    // The escape and enter bindings, on the popup that traps the focus. They
    // run the same two handlers the Cancel and OK buttons carry, which is
    // what Rust's on_action pair does with a ClickEvent::default().
    if (keyboard) {
        DialogBindKeys(cx, popup, trap, onCancel, onOk, onClose);
    }
    return gpui::Dialog::New(cx)
        ->Trap(trap)
        ->Backdrop(backdrop)
        ->Popup(popup)
        ->IntoEl()
        // `.with_animation("fade-in", .., |this, delta| this.opacity(delta))`:
        // the backdrop and the panel fade in together, as one layer.
        ->Opacity(delta);
}

} // namespace component
} // namespace gpui
