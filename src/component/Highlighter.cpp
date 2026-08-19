#include "component/Highlighter.h"

namespace gpui {

namespace component {

Highlighter* Highlighter::New(Ctx* cx, InputState* state) {
    Arena* a = cx->a;
    Highlighter* h = ArenaNew<Highlighter>(a);
    h->a = a;
    h->cx = cx;
    h->state = state;
    return h;
}

El* Highlighter::IntoEl() {
    const Theme& th = cx->theme();
    InputEditorStyle style;
    style.foreground = th.foreground;
    style.mutedForeground = th.mutedFg;
    style.caret = th.caret;
    style.selection = RgbaOpacity(th.accent, 0.45f);
    style.fontSize = 12;
    return gpui::Editor::New(cx, state, style);
}

} // namespace component
} // namespace gpui
