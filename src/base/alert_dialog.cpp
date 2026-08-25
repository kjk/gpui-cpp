#include "base/alert_dialog.h"
#include "base/focus_trap.h"
#include "base/element_ext.h"

namespace gpui {

El* AlertDialogBackdrop::New(Ctx* cx) {
    Arena* a = cx->a;
    return UiRoot(a, StrL("alert-dialog-backdrop"), 0);
}
El* AlertDialogPopup::New(Ctx* cx) {
    Arena* a = cx->a;
    return UiRoot(a, StrL("alert-dialog-popup"), 0);
}
El* AlertDialogTitle::New(Ctx* cx) {
    Arena* a = cx->a;
    return UiRoot(a, StrL("alert-dialog-title"), 0);
}
El* AlertDialogDescription::New(Ctx* cx) {
    Arena* a = cx->a;
    return UiRoot(a, StrL("alert-dialog-description"), 0);
}
// The two answer parts. Rust gives each a click that dispatches Cancel or
// Confirm, which is the same action the escape and enter bindings raise, so
// they end in the dialog's own handlers rather than in one of their own.
El* AlertDialogCancel::New(Ctx* cx) {
    Arena* a = cx->a;
    Str id = StrL("alert-dialog-cancel");
    // `div().id(..).on_click(..)` and nothing else: Rust's close parts carry
    // no focus handle, so the dialog's own is what a keystroke reaches and
    // neither button shows a focus ring when the dialog opens.
    return Div(a)->PathClick(id)->OnClickAction(action::Cancel());
}
El* AlertDialogAction::New(Ctx* cx) {
    Arena* a = cx->a;
    Str id = StrL("alert-dialog-action");
    // `Confirm { secondary: false }`, which is the plain payload.
    return Div(a)->PathClick(id)->OnClickAction(action::Confirm());
}
// The trigger takes the press, not the click, as its Rust counterpart does.
El* AlertDialogTrigger::New(Ctx* cx, Listener onOpen) {
    Arena* a = cx->a;
    El* e = Div(a);
    if (onOpen.IsValid()) {
        e->OnMouseDown(onOpen);
    }
    return e;
}

AlertDialog* AlertDialog::New(Ctx* cx) {
    Arena* a = cx->a;
    AlertDialog* d = ArenaNew<AlertDialog>(a);
    // Viewport host, like Rust Dialog's deferred+anchored overlay.
    d->cx = cx;
    d->trap = StrL("alert-dialog");
    d->root = Div(a)->Fixed()->Top(0)->Left(0)->W(kFill)->H(kFill)->FlexCol();
    return d;
}

AlertDialog* AlertDialog::Trap(Str name) {
    trap = name;
    return this;
}

AlertDialog* AlertDialog::Backdrop(El* backdrop) {
    if (backdrop) {
        root->Child(backdrop);
    }
    return this;
}

AlertDialog* AlertDialog::Popup(El* popup) {
    if (popup) {
        int id = FocusTrapId(trap);
        popup->TrapId(id);
        FocusTrapArm(cx->win, id);
        root->Child(popup);
    }
    return this;
}

El* AlertDialog::IntoEl() {
    return root;
}
} // namespace gpui
