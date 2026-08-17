#include "ui/Popup.h"
#include "ui/Primitive.h"

Popup* Popup::New(Arena* a, Str id, El* trigger) {
    Popup* p = ::New<Popup>(a);
    // Root sizes to the trigger only. Content is an overlay (Rust Positioner).
    p->root = UiRoot(a, id, 0);
    if (trigger) {
        p->root->Child(trigger);
    }
    return p;
}

Popup* Popup::Content(El* content) {
    if (content) {
        // Sit under the trigger. In-flow content would grow the centered
        // showcase page and jump the trigger; overlaying Top(0) covers it
        // so a second click cannot dismiss.
        if (!content->style.absolute) {
            content->AnchorBelow(4)->Left(0);
        }
        root->Child(content);
    }
    return this;
}

El* Popup::IntoEl() {
    return root;
}
