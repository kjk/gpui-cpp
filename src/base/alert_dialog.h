/* Unstyled alert dialog — crates/base/src/alert_dialog.rs */

#include "gpui/gpui.h"
#include "base/dialog.h"

namespace gpui {

// AlertDialog::new is `Dialog::new(cx).close_on_backdrop_press(false)`: an
// alert is a question the reader has to answer, so the backdrop never
// dismisses it. Everything else about it is a Dialog, including the escape and
// enter bindings, which DialogActionForKey answers for.
const bool kAlertDialogClosesOnBackdropPress = false;

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
// Both parts dispatch an action rather than carrying a handler of their own:
// Rust's Cancel and Action wrappers call window.dispatch_action, so the
// dialog's own Cancel and Confirm handlers are what run. Passing the same
// listener the keyboard path uses is how that reads here.
struct AlertDialogCancel {
    static El* New(Ctx* cx, Listener onCancel = {});
};
struct AlertDialogAction {
    static El* New(Ctx* cx, Listener onConfirm = {});
};
struct AlertDialogTrigger {
    static El* New(Ctx* cx, Listener onOpen = {});
};

struct AlertDialog {
    Ctx* cx = nullptr;
    El* root = nullptr;
    // An alert is a Dialog, so it traps focus like one — under its own name,
    // since an alert is what a dialog opens on top of.
    Str trap = {};

    static AlertDialog* New(Ctx* cx);
    AlertDialog* Trap(Str name);
    AlertDialog* Backdrop(El* backdrop);
    AlertDialog* Popup(El* popup);
    El* IntoEl();
};
} // namespace gpui
