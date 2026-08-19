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
El* AlertDialogCancel::New(Ctx* cx) {
    Arena* a = cx->a;
    return UiRoot(a, StrL("alert-dialog-cancel"), 0);
}
El* AlertDialogAction::New(Ctx* cx) {
    Arena* a = cx->a;
    return UiRoot(a, StrL("alert-dialog-action"), 0);
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
