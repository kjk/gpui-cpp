#include "component/Highlighter.h"

namespace component {

Highlighter* Highlighter::New(Arena* a, const char* text) {
    Highlighter* h = ::New<Highlighter>(a);
    h->a = a;
    h->text = text;
    return h;
}

El* Highlighter::IntoEl() {
    return ::Editor::New(a, text);
}

} // namespace component
