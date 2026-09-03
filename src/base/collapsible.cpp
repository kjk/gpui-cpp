#include "base/collapsible.h"

namespace gpui {

Collapsible* Collapsible::New(Ctx* cx) {
    Arena* a = cx->a;
    Collapsible* c = ArenaNew<Collapsible>(a);
    c->cx = cx;
    c->root = Div(a);
    return c;
}

Collapsible* Collapsible::FlexCol() {
    root->FlexCol();
    return this;
}

Collapsible* Collapsible::Open(bool v) {
    open = v;
    return this;
}

Collapsible* Collapsible::Reveal(Str id, float progress) {
    revealId = id;
    revealProgress = progress;
    hasReveal = true;
    return this;
}

Collapsible* Collapsible::Child(El* e) {
    root->Child(e);
    return this;
}

Collapsible* Collapsible::Content(El* e) {
    if (!e) {
        return this;
    }
    if (hasReveal) {
        root->Child(MotionReveal::New(cx, revealId, revealProgress, e));
    } else if (open) {
        root->Child(e);
    }
    return this;
}

El* Collapsible::IntoEl() {
    return root;
}
} // namespace gpui
