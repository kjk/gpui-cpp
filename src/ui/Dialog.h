/* Unstyled dialog — crates/base/src/dialog.rs */

#include "gpui/Gpui.h"

namespace gpui {

struct DialogBackdrop {
    static El* New(Ctx* cx);
};
struct DialogPopup {
    static El* New(Ctx* cx);
};
struct DialogTitle {
    static El* New(Ctx* cx);
};
struct DialogDescription {
    static El* New(Ctx* cx);
};
struct DialogClose {
    static El* New(Ctx* cx, int clickId = 0);
};

struct Dialog {
    El* root = nullptr;

    static Dialog* New(Ctx* cx);
    Dialog* Backdrop(El* backdrop);
    Dialog* Popup(El* popup);
    El* IntoEl();
};
} // namespace gpui
