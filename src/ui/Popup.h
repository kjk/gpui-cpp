/* Unstyled popup — crates/base/src/popup.rs */

#include "gpui/Gpui.h"

namespace gpui {

struct Popup {
    El* root = nullptr;
    // Anchor::TopRight rather than the default TopLeft: the content's right
    // edge lines up with the trigger's, which is what the story toolbar's
    // dropdowns and any menu near the right edge want.
    bool anchorRight = false;

    static Popup* New(Ctx* cx, Str id, El* trigger);
    Popup* AnchorRight(bool on = true);
    Popup* Content(El* content);
    El* IntoEl();
};
} // namespace gpui
