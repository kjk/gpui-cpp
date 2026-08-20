#include "ui/highlighter.h"
#include "ui/input.h"

namespace gpui {

namespace component {

Highlighter* Highlighter::New(Ctx* cx, InputState* state) {
    return New(cx, StrL("editor"), state);
}
Highlighter* Highlighter::New(Ctx* cx, Str id, InputState* state) {
    Arena* a = cx->a;
    Highlighter* h = ArenaNew<Highlighter>(a);
    h->a = a;
    h->cx = cx;
    h->id = id;
    h->state = state;
    return h;
}
Highlighter* Highlighter::H(float v) {
    h = v;
    return this;
}

El* Highlighter::IntoEl() {
    const Theme& th = cx->theme();
    InputEditorStyle style;
    style.foreground = th.foreground;
    style.mutedForeground = th.mutedFg;
    style.caret = th.caret;
    style.selection = RgbaOpacity(th.accent, 0.45f);
    style.fontSize = 12;
    El* editor = gpui::Editor::New(cx, state, style);
    if (h <= 0) {
        return editor;
    }
    // The scroll handle is the editor's: the rows slide under this box as the
    // caret moves, and the wheel moves them too.
    return InputBase::New(cx, id, HashClickId(id))
        ->BindInput(state)
        ->FlexCol()
        ->W(kFill)
        ->H(h)
        ->ClipY()
        ->ScrollY(state ? state->scrollY : 0)
        ->Child(editor);
}

} // namespace component
} // namespace gpui
