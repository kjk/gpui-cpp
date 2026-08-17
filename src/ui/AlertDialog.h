/* Unstyled alert dialog — crates/base/src/alert_dialog.rs */

#include "gpui/Gpui.h"

namespace gpui {

struct AlertDialogBackdrop {
    static El* New(Arena* a);
};
struct AlertDialogPopup {
    static El* New(Arena* a);
};
struct AlertDialogTitle {
    static El* New(Arena* a);
};
struct AlertDialogDescription {
    static El* New(Arena* a);
};
struct AlertDialogCancel {
    static El* New(Arena* a);
};
struct AlertDialogAction {
    static El* New(Arena* a);
};

struct AlertDialog {
    El* root = nullptr;

    static AlertDialog* New(Arena* a);
    AlertDialog* Backdrop(El* backdrop);
    AlertDialog* Popup(El* popup);
    El* IntoEl();
};
} // namespace gpui
