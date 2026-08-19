#include "base/avatar.h"

namespace gpui {

Avatar* Avatar::New(Ctx* cx) {
    Arena* a = cx->a;
    Avatar* v = ArenaNew<Avatar>(a);
    v->root = Div(a);
    return v;
}

Avatar* Avatar::Size(float px) {
    root->W(px)->H(px);
    return this;
}

Avatar* Avatar::Fallback(El* fb) {
    fallback = fb;
    return this;
}

El* Avatar::IntoEl() {
    if (fallback) {
        root->Child(fallback);
    }
    return root;
}

El* AvatarFallback::New(Ctx* cx) {
    Arena* a = cx->a;
    return Div(a);
}
} // namespace gpui
