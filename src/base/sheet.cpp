#include "base/sheet.h"

namespace gpui {

Sheet* Sheet::New(Ctx* cx) {
    Arena* a = cx->a;
    Sheet* s = ArenaNew<Sheet>(a);
    s->root = Div(a)->Fixed()->Top(0)->Left(0)->W(kFill)->H(kFill);
    return s;
}

Sheet* Sheet::Overlay(El* overlay) {
    if (overlay) {
        root->Child(overlay);
    }
    return this;
}

Sheet* Sheet::Surface(El* surface) {
    if (surface) {
        root->Child(surface);
    }
    return this;
}

El* Sheet::IntoEl() {
    return root;
}
} // namespace gpui
