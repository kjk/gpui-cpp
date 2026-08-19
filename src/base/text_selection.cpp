#include "base/text_selection.h"
#include "base/element_ext.h"

namespace gpui {

void TextSelectionBegin(TextSelectionGesture* g, bool insideText) {
    g->selecting = true;
    // A fresh gesture starts over: Rust assigns rather than ORs here, so the
    // previous drag's hit does not carry into this one.
    g->didHitText = insideText;
}

void TextSelectionExtend(TextSelectionGesture* g, bool insideText) {
    if (!g->selecting) {
        return;
    }
    // |=, never cleared: once any point has landed on text the selection
    // stands, even as the pointer wanders back into the margin.
    g->didHitText = g->didHitText || insideText;
}

void TextSelectionEnd(TextSelectionGesture* g) {
    g->selecting = false;
}

bool TextSelectionPublishes(const TextSelectionGesture* g) {
    return g->didHitText;
}

void TextSelectionClear(TextSelectionGesture* g) {
    g->selecting = false;
    g->didHitText = false;
}

El* TextSelection::New(Ctx* cx, Str id, int clickId) {
    Arena* a = cx->a;
    return UiRoot(a, id, clickId);
}
} // namespace gpui
