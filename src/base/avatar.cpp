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

Avatar* Avatar::Image(El* e) {
    image = e;
    return this;
}

Avatar* Avatar::Fallback(El* fb) {
    fallback = fb;
    return this;
}

El* Avatar::IntoEl() {
    // image.or_else(fallback): one child at most, and the picture wins.
    El* content = image ? image : fallback;
    if (content) {
        root->Child(content);
    }
    return root;
}

El* AvatarImage::New(Ctx* cx) {
    Arena* a = cx->a;
    return Div(a);
}

El* AvatarFallback::New(Ctx* cx) {
    Arena* a = cx->a;
    return Div(a);
}
} // namespace gpui
