#include "component/Highlighter.h"

namespace gpui {

namespace component {

Highlighter* Highlighter::New(Ctx* cx, const char* text) {
    Arena* a = cx->a;
    Highlighter* h = ArenaNew<Highlighter>(a);
    h->a = a;
    h->cx = cx;
    h->text = text;
    return h;
}

El* Highlighter::IntoEl() {
    return gpui::Editor::New(cx, text);
}

} // namespace component
} // namespace gpui
