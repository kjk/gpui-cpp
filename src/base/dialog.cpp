#include "base/dialog.h"
#include "base/element_ext.h"
#include "base/focus_trap.h"
#include "base/actions.h"
#include "gpui/keymap.h"

namespace gpui {

Str DialogContext() {
    return StrL("Dialog");
}

void DialogInitKeys() {
    static uint32_t bound = 0;
    if (bound == KeymapGeneration()) {
        return;
    }
    bound = KeymapGeneration();
    const char* ctx = "Dialog";
    KeyBinding bindings[] = {
        {"escape", action::Cancel(), ctx},
        {"enter", action::Confirm(), ctx},
    };
    KeymapBind(bindings, (int)(sizeof(bindings) / sizeof(bindings[0])));
}

DialogAction DialogActionOf(uint32_t id) {
    if (id == action::Cancel()) {
        return DialogAction::Cancel;
    }
    if (id == action::Confirm()) {
        return DialogAction::Confirm;
    }
    return DialogAction::None;
}

void DialogKeys::OnAction(DialogKeys* self, Ctx* cx, const ActionEvent* ev) {
    if (!self) {
        return;
    }
    Listener l = {};
    switch (DialogActionOf(ev->action)) {
        case DialogAction::Cancel:
            l = self->onCancel.IsValid() ? self->onCancel : self->onClose;
            break;
        case DialogAction::Confirm:
            l = self->onOk;
            break;
        default:
            break;
    }
    if (!l.IsValid()) {
        // Nothing to run. Rust's handler still consumes the keystroke — the
        // dialog is modal — so this does too, rather than propagating into
        // whatever is behind the backdrop.
        return;
    }
    // `let event = ClickEvent::default(); confirm(&event, window, cx)`: the
    // keyboard runs the same handler the button does.
    ClickEvent click = {};
    ListenerCall(cx->app, cx->win, l, &click);
}

void DialogBindKeys(Ctx* cx, El* popup, Str name, Listener onCancel,
                    Listener onOk, Listener onClose) {
    if (!cx || !popup) {
        return;
    }
    DialogInitKeys();
    Entity<DialogKeys> keys =
        ElementStateEntity<DialogKeys>(cx, name, StrL("gpui::DialogKeys"));
    if (DialogKeys* k = keys.Get(cx)) {
        k->onCancel = onCancel;
        k->onOk = onOk;
        k->onClose = onClose;
    }
    // track_focus(&self.focus): the host is focusable so that a dialog with
    // nothing focusable inside it still has somewhere for focus to be — and
    // not a tab stop, so Tab still visits the controls rather than the box
    // around them.
    Listener onAction = ListenTo(keys, &DialogKeys::OnAction);
    popup->KeyContext(DialogContext())
        ->FocusId(HashClickId(name))
        ->TabStop(false)
        ->OnAction(action::Cancel(), onAction)
        ->OnAction(action::Confirm(), onAction);
}

bool DialogBackdropCloses(bool overlayClosable, bool topmost,
                          MouseButton button, float pressY,
                          float dismissBelowY) {
    // Above the reserved band the press is not the backdrop's — that is where
    // a title bar a dialog was opened over still is.
    if (pressY < dismissBelowY) {
        return false;
    }
    // Rust computes `overlay_closable && topmost` once, so a dialog under
    // another one never answers a backdrop press.
    return button == MouseButton::Left && overlayClosable && topmost;
}

El* DialogTrigger::New(Ctx* cx, Listener onOpen) {
    Arena* a = cx->a;
    El* e = Div(a);
    if (onOpen.IsValid()) {
        e->OnMouseDown(onOpen);
    }
    return e;
}

El* DialogBackdrop::New(Ctx* cx) {
    Arena* a = cx->a;
    return UiRoot(a, StrL("dialog-backdrop"), 0);
}
El* DialogPopup::New(Ctx* cx) {
    Arena* a = cx->a;
    return UiRoot(a, StrL("dialog-popup"), 0);
}
El* DialogTitle::New(Ctx* cx) {
    Arena* a = cx->a;
    return UiRoot(a, StrL("dialog-title"), 0);
}
El* DialogDescription::New(Ctx* cx) {
    Arena* a = cx->a;
    return UiRoot(a, StrL("dialog-description"), 0);
}
El* DialogClose::New(Ctx* cx, int clickId) {
    Arena* a = cx->a;
    return UiRoot(a, StrL("dialog-close"), clickId);
}

Dialog* Dialog::New(Ctx* cx) {
    Arena* a = cx->a;
    Dialog* d = ArenaNew<Dialog>(a);
    d->cx = cx;
    d->trap = StrL("dialog");
    d->root = Div(a)->Fixed()->Top(0)->Left(0)->W(kFill)->H(kFill);
    return d;
}

Dialog* Dialog::Trap(Str name) {
    trap = name;
    return this;
}

Dialog* Dialog::Backdrop(El* backdrop) {
    if (backdrop) {
        root->Child(backdrop);
    }
    return this;
}

Dialog* Dialog::Popup(El* popup) {
    if (popup) {
        // The popup is the trap container, not the backdrop: a Tab inside a
        // dialog reaches its own controls and nothing behind it.
        int id = FocusTrapId(trap);
        popup->TrapId(id);
        // The popup is also the host DialogBindKeys made focusable, under the
        // same name, so a dialog with no control in it still takes the focus.
        FocusTrapArm(cx->win, id, id);
        root->Child(popup);
    }
    return this;
}

El* Dialog::IntoEl() {
    return root;
}
} // namespace gpui
