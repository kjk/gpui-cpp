#include "base/hover_card.h"
#include "base/element_ext.h"

namespace gpui {

HoverCard* HoverCard::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
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
        // Under the trigger unless the caller already placed it.
        if (!content->style.absolute) {
            content->Absolute()->Top(22)->Left(0);
        }
        root->Child(content);
    }
    return this;
}

El* HoverCard::IntoEl() {
    return root;
}
} // namespace gpui
