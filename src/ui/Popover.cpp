#include "ui/Popover.h"
#include "ui/Primitive.h"

namespace gpui {

Popover* Popover::New(Arena* a, Str id) {
    Popover* p = ArenaNew<Popover>(a);
    p->root = UiRoot(a, id, 0);
    return p;
}

Popover* Popover::Trigger(El* trigger) {
    if (trigger) {
        root->Child(trigger);
    }
    return this;
}

Popover* Popover::Content(El* content) {
    if (content) {
        content->Absolute()->Top(28)->Left(0);
        root->Child(content);
    }
    return this;
}

El* Popover::IntoEl() {
    return root;
}
} // namespace gpui
