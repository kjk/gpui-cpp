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

// ─── the window's selection ───────────────────────────────────────────────

WindowSelection* WindowSelectionOf(Window* win) {
    if (!win) {
        return nullptr;
    }
    if (!win->sel) {
        win->sel = new WindowSelection();
    }
    return win->sel;
}

void WindowSelectionFree(Window* win) {
    if (win && win->sel) {
        delete win->sel;
        win->sel = nullptr;
    }
}

void WindowSelectionClear(Window* win) {
    WindowSelection* s = win ? win->sel : nullptr;
    if (!s) {
        return;
    }
    TextSelectionClear(&s->gesture);
    s->anchor = -1;
    s->cursor = -1;
    s->scope = 0;
}

bool WindowSelectionHas(const Window* win) {
    const WindowSelection* s = win ? win->sel : nullptr;
    if (!s || s->anchor < 0 || s->cursor < 0 || s->anchor == s->cursor) {
        return false;
    }
    return TextSelectionPublishes(&s->gesture);
}

void WindowSelectionPress(Window* win, float x, float y, int clickCount,
                          bool extend) {
    WindowSelection* s = WindowSelectionOf(win);
    if (!s) {
        return;
    }
    PaintCtx* ctx = &win->paint;
    // A shift-click keeps the anchor and moves the cursor — Rust's
    // `begin_in_window(.., extend)` — and stays in the scope it began in.
    if (extend && s->anchor >= 0) {
        int off = TextHitOffsetIn(ctx, x, y, true, s->scope, nullptr);
        if (off >= 0) {
            s->cursor = off;
            TextSelectionExtend(
                &s->gesture,
                TextHitOffsetIn(ctx, x, y, false, s->scope, nullptr) >= 0);
        }
        return;
    }
    // Two clicks take the word under the pointer, three the whole run —
    // points_for_multi_click. A multi-click lands on a glyph by definition,
    // so the gesture has hit text; the drag does not extend it, because the
    // selection is already the unit that was asked for.
    int scope = 0;
    int a = 0;
    int b = 0;
    if (TextMultiClickRangeIn(ctx, x, y, clickCount, -1, &a, &b, &scope)) {
        s->scope = scope;
        s->anchor = a;
        s->cursor = b;
        TextSelectionBegin(&s->gesture, true);
        TextSelectionEnd(&s->gesture);
        return;
    }
    // A press in the margin still begins a gesture, so a drag from beside a
    // paragraph into it selects; whether it was on a glyph is what decides
    // if anything is ever published.
    int anchor = TextHitOffsetIn(ctx, x, y, true, -1, &scope);
    if (anchor >= 0) {
        s->scope = scope;
        s->anchor = anchor;
        s->cursor = anchor;
        TextSelectionBegin(&s->gesture, TextHitOffsetIn(ctx, x, y, false, scope,
                                                        nullptr) >= 0);
        return;
    }
    WindowSelectionClear(win);
}

void WindowSelectionDrag(Window* win, float x, float y) {
    WindowSelection* s = win ? win->sel : nullptr;
    if (!s || !s->gesture.selecting) {
        return;
    }
    PaintCtx* ctx = &win->paint;
    // did_hit_text is the strict hit — whether *this* point is on a glyph —
    // while the offset the selection runs to is the nearest one either way,
    // so a drag through the margin keeps going along the line.
    TextSelectionExtend(
        &s->gesture, TextHitOffsetIn(ctx, x, y, false, s->scope, nullptr) >= 0);
    int off = TextHitOffsetIn(ctx, x, y, true, s->scope, nullptr);
    if (off >= 0) {
        s->cursor = off;
    }
}

void WindowSelectionRelease(Window* win) {
    WindowSelection* s = win ? win->sel : nullptr;
    if (s) {
        TextSelectionEnd(&s->gesture);
    }
}

int WindowSelectionText(Window* win, char* out, int cap) {
    if (!WindowSelectionHas(win)) {
        if (out && cap > 0) {
            out[0] = 0;
        }
        return 0;
    }
    WindowSelection* s = win->sel;
    return CopyTextHitsIn(&win->paint, s->anchor, s->cursor, s->scope, out,
                          cap);
}

bool WindowSelectionCopy(Window* win) {
    if (!WindowSelectionHas(win)) {
        return false;
    }
    // One frame's worth of selectable text; a document longer than this
    // copies its first 64 KB rather than nothing.
    const int kCap = 64 * 1024;
    char* buf = (char*)Alloc(nullptr, kCap);
    if (!buf) {
        return false;
    }
    int n = WindowSelectionText(win, buf, kCap);
    if (n > 0) {
        ClipboardSetText(win, Str(buf, n));
    }
    Free(nullptr, buf);
    return n > 0;
}

void WindowSelectionApply(Window* win) {
    if (!win) {
        return;
    }
    WindowSelection* s = win->sel;
    // did_hit_text gates the whole thing: a gesture that never touched a
    // glyph shows nothing, however far it dragged.
    bool publishes = s && TextSelectionPublishes(&s->gesture);
    win->paint.selA = publishes ? s->anchor : -1;
    win->paint.selB = publishes ? s->cursor : -1;
    win->paint.selScope = publishes ? s->scope : -1;
}
} // namespace gpui
