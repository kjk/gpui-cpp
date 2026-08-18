#include "ui/Popup.h"
#include "ui/Primitive.h"

namespace gpui {

Popup* Popup::New(Ctx* cx, Str id, El* trigger) {
    Arena* a = cx->a;
    Popup* p = ArenaNew<Popup>(a);
    // Root sizes to the trigger only. Content is an overlay (Rust Positioner).
    p->root = UiRoot(a, id, 0);
    if (trigger) {
        p->root->Child(trigger);
    }
    return p;
}

Popup* Popup::AnchorRight(bool on) {
    anchorRight = on;
    return this;
}

Popup* Popup::Content(El* content) {
    if (content) {
        // Sit under the trigger. In-flow content would grow the centered
        // showcase page and jump the trigger; overlaying Top(0) covers it
        // so a second click cannot dismiss.
        if (!content->style.absolute) {
            content->AnchorBelow(4);
            if (anchorRight) {
                content->Right(0);
            } else {
                content->Left(0);
            }
        }
        // Rust puts popover content in a deferred layer, so it draws over
        // whatever follows the trigger in the tree rather than under it.
        content->Deferred();
        root->Child(content);
    }
    return this;
}

El* Popup::IntoEl() {
    return root;
}
} // namespace gpui
