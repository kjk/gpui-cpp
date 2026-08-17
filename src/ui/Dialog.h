/* Unstyled dialog — crates/base/src/dialog.rs */

#pragma once

#include "gpui/Gpui.h"

namespace gpui {

struct DialogBackdrop {
    static El* New(Arena* a);
};
struct DialogPopup {
    static El* New(Arena* a);
};
struct DialogTitle {
    static El* New(Arena* a);
};
struct DialogDescription {
    static El* New(Arena* a);
};
struct DialogClose {
    static El* New(Arena* a, int clickId = 0);
};

struct Dialog {
    El* root = nullptr;

    static Dialog* New(Arena* a);
    Dialog* Backdrop(El* backdrop);
    Dialog* Popup(El* popup);
    El* IntoEl();
};
} // namespace gpui
