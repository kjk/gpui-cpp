#include "ui/Avatar.h"

Avatar* Avatar::New(Arena* a) {
    Avatar* v = ::New<Avatar>(a);
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

El* AvatarFallback::New(Arena* a) {
    return Div(a);
}
