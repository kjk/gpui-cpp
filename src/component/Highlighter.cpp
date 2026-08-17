#include "component/Highlighter.h"

namespace gpui {

namespace component {

Highlighter* Highlighter::New(Arena* a, const char* text) {
    Highlighter* h = ArenaNew<Highlighter>(a);
    h->a = a;
    h->text = text;
    return h;
}

El* Highlighter::IntoEl() {
    return gpui::Editor::New(a, text);
}

} // namespace component
} // namespace gpui
