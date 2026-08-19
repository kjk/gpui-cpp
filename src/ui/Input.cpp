#include "ui/Input.h"
#include "ui/Primitive.h"

namespace gpui {

El* InputBase::New(Ctx* cx, Str id, int clickId) {
    Arena* a = cx->a;
    return UiRoot(a, id, clickId);
}

// Input::LINE_HEIGHT is 1.25rem — 20 px at the 16 px root, whatever the text
// size is, rather than the phi box every other line of text gets.
static const float kInputLineH = 20.f;

// One bullet per character, not per byte, for a masked field.
static Str MaskedRun(Arena* a, Str text) {
    int chars = 0;
    for (int i = 0; i < text.len; i++) {
        if (((unsigned char)text.s[i] & 0xc0) != 0x80) {
            chars++;
        }
    }
    char* dots = (char*)Alloc(a, chars * 3 + 1);
    int n = 0;
    for (int i = 0; i < chars; i++) {
        memcpy(dots + n, "\xE2\x80\xA2", 3); // U+2022 BULLET
        n += 3;
    }
    dots[n] = 0;
    return Str(dots, n);
}

// A byte offset into the text, in the bullets that stand in for it: three
// bytes per character, so the caret and the selection land between bullets.
static int MaskedOffset(Str text, int off) {
    int chars = 0;
    for (int i = 0; i < off && i < text.len; i++) {
        if (((unsigned char)text.s[i] & 0xc0) != 0x80) {
            chars++;
        }
    }
    return chars * 3;
}

El* Input::New(Ctx* cx, InputState* state) {
    return New(cx, state, InputEditorStyle{});
}

El* Input::New(Ctx* cx, InputState* state, const InputEditorStyle& style) {
    Arena* a = cx->a;
    if (!state) {
        return TextEl(a, Str{});
    }
    float font = style.fontSize > 0 ? style.fontSize : 12.f;
    float lineMult = kInputLineH / font;
    state->lastLineH = kInputLineH;
    Str text = InputValue(state);
    bool masked = style.mask || state->masked;
    // show_cursor: focused, not disabled, and this half of the blink is the
    // lit one.
    bool caret =
        state->focused && !state->disabled && BlinkVisible(cx, state->blink);

    // The row fills its field, so a press to the right of the text still
    // lands on the editor — Rust's InputElement takes the whole content box.
    El* row = Div(a)
                  ->FlexRow()
                  ->ItemsCenter()
                  ->H(kInputLineH)
                  ->Grow()
                  ->BindInput(state);
    if (style.align == 1) {
        row->W(kFill)->JustifyCenter();
    } else if (style.align == 2) {
        row->W(kFill)->JustifyEnd();
    }

    if (text.len == 0) {
        // The cue takes the muted color and the caret sits at the left edge of
        // the row, so the placeholder is not pushed aside by it.
        if (caret) {
            row->Caret(0, style.caret);
        }
        Str cue = state->placeholder;
        return row->Child(TextEl(a, cue)->Font(font)->LineHeight(lineMult)->Fg(
            style.mutedForeground));
    }

    Str run = masked ? MaskedRun(a, text) : text;
    int cursor = InputCursor(state);
    Selection sel = state->selectedRange;
    if (masked) {
        cursor = MaskedOffset(text, cursor);
        sel.start = MaskedOffset(text, sel.start);
        sel.end = MaskedOffset(text, sel.end);
    }
    El* el = TextEl(a, run)
                 ->Font(font)
                 ->LineHeight(lineMult)
                 ->Fg(style.foreground)
                 ->BindInput(state);
    if (!sel.IsEmpty()) {
        el->SelRange(sel.start, sel.end, style.selection);
    }
    if (caret) {
        el->Caret(cursor, style.caret);
    }
    return row->Child(el);
}

El* Textarea::New(Ctx* cx, InputState* state) {
    return New(cx, state, InputEditorStyle{});
}

// The multi-line editor. Rust lays every visible row out through the display
// map; without one, each logical line is its own run and the selection is
// clipped to it — which is the same picture as long as nothing soft-wraps.
El* Textarea::New(Ctx* cx, InputState* state, const InputEditorStyle& style,
                  bool lineNumbers) {
    Arena* a = cx->a;
    if (!state) {
        return TextEl(a, Str{});
    }
    float font = style.fontSize > 0 ? style.fontSize : 12.f;
    float lineMult = kInputLineH / font;
    state->lastLineH = kInputLineH;
    Str text = InputValue(state);
    bool caret =
        state->focused && !state->disabled && BlinkVisible(cx, state->blink);
    int cursor = InputCursor(state);
    Selection sel = state->selectedRange;

    El* col = Div(a)->FlexCol()->W(kFill)->BindInput(state);
    if (text.len == 0) {
        if (caret) {
            col->Caret(0, style.caret);
        }
        return col->Child(TextEl(a, state->placeholder)
                              ->Font(font)
                              ->LineHeight(lineMult)
                              ->Fg(style.mutedForeground));
    }

    int rows = RopeLinesLen(text);
    float numW = 0;
    if (lineNumbers) {
        numW = 12.f + 7.f * (rows >= 100 ? 3 : (rows >= 10 ? 2 : 1));
    }
    for (int row = 0; row < rows; row++) {
        int start = RopeLineStartOffset(text, row);
        Str line = RopeSliceLine(text, row);
        El* el = TextEl(a, line)->Font(font)->LineHeight(lineMult)->Fg(
            style.foreground);
        if (state->softWrap) {
            el->Wrap();
        }
        // The first row is the one the state measures against; every row below
        // it is a whole lastLineH further down.
        if (row == 0) {
            el->BindInput(state);
        }
        int lo = sel.start - start;
        int hi = sel.end - start;
        if (lo < 0) {
            lo = 0;
        }
        if (hi > line.len) {
            hi = line.len;
        }
        if (!sel.IsEmpty() && lo < hi) {
            el->SelRange(lo, hi, style.selection);
        }
        if (caret && cursor >= start && cursor <= start + line.len) {
            el->Caret(cursor - start, style.caret);
        }
        if (!lineNumbers) {
            col->Child(el);
            continue;
        }
        El* band = Div(a)->FlexRow()->W(kFill)->H(kInputLineH)->Gap(8);
        band->Child(Div(a)->W(numW)->JustifyEnd()->Child(
            TextEl(a, StrDup(a, fmt("%d", row + 1)))
                ->Font(font - 1)
                ->LineHeight(lineMult)
                ->Fg(style.mutedForeground)));
        band->Child(el);
        col->Child(band);
    }
    return col;
}

El* Editor::New(Ctx* cx, InputState* state) {
    return New(cx, state, InputEditorStyle{});
}

El* Editor::New(Ctx* cx, InputState* state, const InputEditorStyle& style) {
    // EditorMode is TextareaMode plus the language features this tree does not
    // have; what is left of it that we do render is the line number gutter.
    return Textarea::New(cx, state, style, true);
}

} // namespace gpui
