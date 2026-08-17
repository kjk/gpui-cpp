#include "ui/Dialog.h"
#include "ui/Primitive.h"

namespace gpui {

El* DialogBackdrop::New(Arena* a) {
    return UiRoot(a, StrL("dialog-backdrop"), 0);
}
El* DialogPopup::New(Arena* a) {
    return UiRoot(a, StrL("dialog-popup"), 0);
}
El* DialogTitle::New(Arena* a) {
    return UiRoot(a, StrL("dialog-title"), 0);
}
El* DialogDescription::New(Arena* a) {
    return UiRoot(a, StrL("dialog-description"), 0);
}
El* DialogClose::New(Arena* a, int clickId) {
    return UiRoot(a, StrL("dialog-close"), clickId);
}

Dialog* Dialog::New(Arena* a) {
    Dialog* d = ArenaNew<Dialog>(a);
    d->root = Div(a)->Fixed()->Top(0)->Left(0)->W(kFill)->H(kFill);
    return d;
}

Dialog* Dialog::Backdrop(El* backdrop) {
    if (backdrop) {
        root->Child(backdrop);
    }
    return this;
}

Dialog* Dialog::Popup(El* popup) {
    if (popup) {
        root->Child(popup);
    }
    return this;
}

El* Dialog::IntoEl() {
    return root;
}
} // namespace gpui
