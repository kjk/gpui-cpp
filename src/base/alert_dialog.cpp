#include "base/alert_dialog.h"
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
El* AlertDialogCancel::New(Ctx* cx, Listener onCancel) {
    Arena* a = cx->a;
    Str id = StrL("alert-dialog-cancel");
    El* e = Div(a)->Id(id)->Click(HashClickId(id))->FocusId(HashClickId(id));
    if (onCancel.IsValid()) {
        e->OnClick(onCancel);
    }
    return e;
}
El* AlertDialogAction::New(Ctx* cx, Listener onConfirm) {
    Arena* a = cx->a;
    Str id = StrL("alert-dialog-action");
    El* e = Div(a)->Id(id)->Click(HashClickId(id))->FocusId(HashClickId(id));
    if (onConfirm.IsValid()) {
        e->OnClick(onConfirm);
    }
    return e;
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
    d->root = Div(a)->Fixed()->Top(0)->Left(0)->W(kFill)->H(kFill)->FlexCol();
    return d;
}

AlertDialog* AlertDialog::Backdrop(El* backdrop) {
    if (backdrop) {
        root->Child(backdrop);
    }
    return this;
}

AlertDialog* AlertDialog::Popup(El* popup) {
    if (popup) {
        root->Child(popup);
    }
    return this;
}

El* AlertDialog::IntoEl() {
    return root;
}
} // namespace gpui
