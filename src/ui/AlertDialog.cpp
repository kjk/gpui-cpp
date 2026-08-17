#include "ui/AlertDialog.h"
#include "ui/Primitive.h"

El* AlertDialogBackdrop::New(Arena* a) {
    return UiRoot(a, StrL("alert-dialog-backdrop"), 0);
}
El* AlertDialogPopup::New(Arena* a) {
    return UiRoot(a, StrL("alert-dialog-popup"), 0);
}
El* AlertDialogTitle::New(Arena* a) {
    return UiRoot(a, StrL("alert-dialog-title"), 0);
}
El* AlertDialogDescription::New(Arena* a) {
    return UiRoot(a, StrL("alert-dialog-description"), 0);
}
El* AlertDialogCancel::New(Arena* a) {
    return UiRoot(a, StrL("alert-dialog-cancel"), 0);
}
El* AlertDialogAction::New(Arena* a) {
    return UiRoot(a, StrL("alert-dialog-action"), 0);
}

AlertDialog* AlertDialog::New(Arena* a) {
    AlertDialog* d = ::New<AlertDialog>(a);
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
