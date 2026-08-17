#include "ui/HoverCard.h"
#include "ui/Primitive.h"

namespace gpui {

HoverCard* HoverCard::New(Arena* a, Str id) {
    HoverCard* h = ArenaNew<HoverCard>(a);
    h->root = UiRoot(a, id, 0);
    return h;
}

HoverCard* HoverCard::Trigger(El* trigger) {
    if (trigger) {
        root->Child(trigger);
    }
    return this;
}

HoverCard* HoverCard::Content(El* content) {
    if (content) {
        content->Absolute()->Top(22)->Left(0);
        root->Child(content);
    }
    return this;
}

El* HoverCard::IntoEl() {
    return root;
}
} // namespace gpui
