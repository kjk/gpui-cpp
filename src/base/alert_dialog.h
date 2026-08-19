/* Unstyled alert dialog — crates/base/src/alert_dialog.rs */

#include "gpui/gpui.h"

namespace gpui {

struct AlertDialogBackdrop {
    static El* New(Ctx* cx);
};
struct AlertDialogPopup {
    static El* New(Ctx* cx);
};
struct AlertDialogTitle {
    static El* New(Ctx* cx);
};
struct AlertDialogDescription {
    static El* New(Ctx* cx);
};
struct AlertDialogCancel {
    static El* New(Ctx* cx);
};
struct AlertDialogAction {
    static El* New(Ctx* cx);
};

struct AlertDialog {
    El* root = nullptr;

    static AlertDialog* New(Ctx* cx);
    AlertDialog* Backdrop(El* backdrop);
    AlertDialog* Popup(El* popup);
    El* IntoEl();
};
} // namespace gpui
