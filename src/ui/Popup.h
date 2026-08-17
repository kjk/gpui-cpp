/* Unstyled popup — crates/base/src/popup.rs */

#include "gpui/Gpui.h"

namespace gpui {

struct Popup {
    El* root = nullptr;

    static Popup* New(Ctx* cx, Str id, El* trigger);
    Popup* Content(El* content);
    El* IntoEl();
};
} // namespace gpui
