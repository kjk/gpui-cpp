/* Port of crates/base/src/input/ — the text field and the editing engine
   behind it. Rust splits the module across input/input/mod.rs,
   input/textarea/mod.rs and input/base/{state,movement,selection,mode,
   mask_pattern,rope_ext,change,undo_manager}.rs; the directory is one file
   here. blink_cursor.rs stays in gpui/gpui.cpp beside the window timers it
   needs. */

#include "base/input.h"
#include "base/element_ext.h"

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
    Selection mark = {};
    bool marking = InputMarkedRange(state, &mark);
    if (marking) {
        // InputElement puts the caret at the end of the marked range and
        // shows no selection inside it: what the input method has staged is
        // one run being composed, not text the user has picked out.
        cursor = mark.end;
        sel = SelectionAt(mark.end);
    }
    if (masked) {
        cursor = MaskedOffset(text, cursor);
        sel.start = MaskedOffset(text, sel.start);
        sel.end = MaskedOffset(text, sel.end);
        mark.start = MaskedOffset(text, mark.start);
        mark.end = MaskedOffset(text, mark.end);
    }
    El* el = TextEl(a, run)
                 ->Font(font)
                 ->LineHeight(lineMult)
                 ->Fg(style.foreground)
                 ->BindInput(state);
    if (!sel.IsEmpty()) {
        el->SelRange(sel.start, sel.end, style.selection);
    }
    if (marking) {
        el->MarkRange(mark.start, mark.end);
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
        El* ph = TextEl(a, state->placeholder)
                     ->Font(font)
                     ->LineHeight(lineMult)
                     ->Fg(style.mutedForeground);
        if (style.mono) {
            ph->Mono();
        }
        return col->Child(ph);
    }

    int rows = RopeLinesLen(text);
    // The scrolled height, which is what scroll_to clamps against.
    state->contentH = (float)rows * kInputLineH;
    float numW = 0;
    if (lineNumbers) {
        numW = 12.f + 7.f * (rows >= 100 ? 3 : (rows >= 10 ? 2 : 1));
    }
    // The row the caret is on, which is the one the active-line wash covers.
    int caretRow = -1;
    if (style.activeLine.a != 0) {
        caretRow = RopeOffsetToPoint(text, cursor).row;
    }
    // A monospace column, for the indent guides. The glyphs are all one width
    // in the family the editor asks for, so one measurement does.
    float colW = 0;
    if (style.indentGuide.a != 0 && style.indentWidth > 0) {
        colW = font * 0.6f;
    }
    // The document's runs, sliced per row below.
    int spanAt = 0;
    for (int row = 0; row < rows; row++) {
        int start = RopeLineStartOffset(text, row);
        Str line = RopeSliceLine(text, row);
        El* el = TextEl(a, line)->Font(font)->LineHeight(lineMult)->Fg(
            style.foreground);
        if (style.mono) {
            el->Mono();
        }
        // The runs that fall inside this row, rebased onto it. The document's
        // are in order, so the walk carries on where the last row left off.
        if (style.nSpans > 0) {
            while (spanAt < style.nSpans && style.spans[spanAt].hi <= start) {
                spanAt++;
            }
            int first = spanAt;
            int count = 0;
            while (first + count < style.nSpans &&
                   style.spans[first + count].lo < start + line.len) {
                count++;
            }
            if (count > 0) {
                auto* rowSpans =
                    (TextSpan*)Alloc(a, (int)sizeof(TextSpan) * count);
                int nRowSpans = 0;
                for (int k = 0; k < count; k++) {
                    const TextSpan& sp = style.spans[first + k];
                    int lo = sp.lo - start;
                    int hi = sp.hi - start;
                    if (lo < 0) {
                        lo = 0;
                    }
                    if (hi > line.len) {
                        hi = line.len;
                    }
                    if (hi <= lo) {
                        continue;
                    }
                    rowSpans[nRowSpans] = sp;
                    rowSpans[nRowSpans].lo = lo;
                    rowSpans[nRowSpans].hi = hi;
                    nRowSpans++;
                }
                if (nRowSpans > 0) {
                    el->Spans(rowSpans, nRowSpans);
                }
            }
        }
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
        // indent_guides: a hairline every tab stop of the row's own leading
        // whitespace, drawn behind the text.
        El* guides = nullptr;
        if (colW > 0) {
            int lead = 0;
            while (lead < line.len && line.s[lead] == ' ') {
                lead++;
            }
            int stops = lead / style.indentWidth;
            if (stops > 0) {
                guides = Div(a)->Absolute()->Left(0)->Top(0)->H(kFill);
                for (int g = 0; g < stops; g++) {
                    guides->Child(
                        Div(a)
                            ->Absolute()
                            ->Left(colW * (float)(g * style.indentWidth))
                            ->Top(0)
                            ->W(1)
                            ->H(kFill)
                            ->Bg(style.indentGuide));
                }
            }
        }
        if (!lineNumbers) {
            if (guides) {
                col->Child(
                    Div(a)->W(kFill)->H(kInputLineH)->Child(guides)->Child(el));
                continue;
            }
            col->Child(el);
            continue;
        }
        El* band = Div(a)->FlexRow()->W(kFill)->H(kInputLineH)->Gap(8);
        // active_line: the wash under the row the caret is on, gutter and all.
        if (row == caretRow) {
            band->Bg(style.activeLine);
        }
        El* num = TextEl(a, StrDup(a, fmt("%d", row + 1)))
                      ->Font(font - 1)
                      ->LineHeight(lineMult)
                      ->Fg(style.mutedForeground);
        if (style.mono) {
            num->Mono();
        }
        band->Child(Div(a)->W(numW)->JustifyEnd()->Child(num));
        if (guides) {
            band->Child(Div(a)->Grow()->H(kFill)->Child(guides)->Child(el));
        } else {
            band->Child(el);
        }
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

/* Port of crates/base/src/input/base — state.rs, movement.rs, selection.rs and
   mode.rs. blink_cursor.rs is in Gpui.cpp beside the window timers it needs,
   rope_ext.rs is Rope.cpp, mask_pattern.rs is MaskPattern.cpp, and change.rs +
   undo_manager.rs are UndoManager.cpp.

   Rust's engine is `InputBaseState<M>`, generic over a mode marker so that a
   method which makes no sense for a single-line field does not exist on it.
   There is no such thing to bound on here, so the marker is a runtime
   `InputKind` and those methods return early — `InputMoveVertical` on an
   `InputKind::Input` is the compile error Rust would have raised.

   What is deliberately not ported, because it needs machinery this tree does
   not have: the IME marked range, the display map (soft wrap, folding, wrapped
   line movement), the LSP and code-editor features, the search session,
   scroll_to and the scrollbars, syntax highlighting, and number stepping. Line
   movement therefore works on logical lines, and start_of_line / end_of_line
   have no "first press goes to the visual line" branch. */

// ─── the document ─────────────────────────────────────────────────────────
//
// Rust holds it in a `ropey::Rope`. Here it is a flat UTF-8 buffer, kept
// NUL-terminated past `len` so a `const char*` reader still works; the
// terminator is not counted in the length.

Str InputValue(const InputState* s) {
    if (!s || s->text.len <= 0) {
        return {};
    }
    return Str(s->text.els, s->text.len);
}

const char* InputCStr(const InputState* s) {
    return s && s->text.els ? s->text.els : "";
}

InputState::~InputState() {
    StrFree(placeholder);
    MaskPatternFree(&maskPattern);
}

static void TextReserve(InputState* s, int want) {
    VecReserve(s->text, want + 1);
}

// Rope::replace, over the flat buffer.
static void TextSplice(InputState* s, int a, int b, Str ins) {
    int len = s->text.len;
    if (a < 0) {
        a = 0;
    }
    if (b > len) {
        b = len;
    }
    if (b < a) {
        b = a;
    }
    int insLen = ins.len > 0 ? ins.len : 0;
    int out = len - (b - a) + insLen;
    TextReserve(s, out);
    if (!s->text.els) {
        return;
    }
    memmove(s->text.els + a + insLen, s->text.els + b, (size_t)(len - b));
    if (insLen > 0) {
        memcpy(s->text.els + a, ins.s, (size_t)insLen);
    }
    s->text.len = out;
    s->text.els[out] = 0;
}

static void TextSet(InputState* s, Str v) {
    int n = v.len > 0 ? v.len : 0;
    TextReserve(s, n);
    if (!s->text.els) {
        return;
    }
    if (n > 0) {
        memmove(s->text.els, v.s, (size_t)n);
    }
    s->text.len = n;
    s->text.els[n] = 0;
}

// ─── mode ─────────────────────────────────────────────────────────────────

void LayoutModeSetRows(LayoutMode* m, int rows) {
    if (m->kind == LayoutModeKind::AutoGrow) {
        int lo = m->minRows > 0 ? m->minRows : 1;
        int hi = m->maxRows > 0 ? m->maxRows : rows;
        m->rows = rows < lo ? lo : (rows > hi ? hi : rows);
        return;
    }
    m->rows = rows;
}

int LayoutModeRows(const LayoutMode& m) {
    return m.rows > 1 ? m.rows : 1; // "At least 1 row be return."
}

int LayoutModeMinRows(const LayoutMode& m) {
    if (m.kind != LayoutModeKind::AutoGrow) {
        return 1;
    }
    return m.minRows > 1 ? m.minRows : 1;
}

bool InputIsMultiLine(const InputState* s) {
    // kind.rs MULTI_LINE. The kind decides this, not the layout: an
    // auto-growing textarea capped at one row is still multi-line.
    return s->kind != InputKind::Input;
}

bool InputIsSingleLine(const InputState* s) {
    return !InputIsMultiLine(s);
}

bool InputIsEditable(const InputState* s) {
    return !s->disabled && !s->readonly;
}

// ─── cursor and selection ─────────────────────────────────────────────────

int InputCursor(const InputState* s) {
    return s->selectionReversed ? s->selectedRange.start : s->selectedRange.end;
}

RopePoint InputCursorPosition(const InputState* s) {
    return RopeOffsetToPoint(InputValue(s), InputCursor(s));
}

Str InputSelectedValue(const InputState* s) {
    Str t = InputValue(s);
    Selection r = s->selectedRange;
    if (r.IsEmpty() || r.start < 0 || r.end > t.len) {
        return {};
    }
    return Str(t.s + r.start, r.end - r.start);
}

Str InputUnmaskValue(Arena* a, const InputState* s) {
    return MaskUnapply(a, s->maskPattern, InputValue(s));
}

int InputPreviousBoundary(const InputState* s, int offset) {
    Str t = InputValue(s);
    int off = RopeClipOffset(t, offset > 0 ? offset - 1 : 0, Bias::Left);
    uint32_t c = 0;
    if (RopeCharAt(t, off, &c) && c == '\r' && off > 0) {
        off--;
    }
    return off;
}

int InputNextBoundary(const InputState* s, int offset) {
    Str t = InputValue(s);
    int off = RopeClipOffset(t, offset + 1, Bias::Right);
    uint32_t c = 0;
    if (RopeCharAt(t, off, &c) && c == '\r' && off < t.len) {
        off++;
    }
    return off;
}

int InputStartOfLine(const InputState* s) {
    if (InputIsSingleLine(s)) {
        return 0;
    }
    Str t = InputValue(s);
    return RopeLineStartOffset(t, RopeOffsetToPoint(t, InputCursor(s)).row);
}

int InputEndOfLine(const InputState* s) {
    Str t = InputValue(s);
    if (InputIsSingleLine(s)) {
        return t.len;
    }
    return RopeLineEndOffset(t, RopeOffsetToPoint(t, InputCursor(s)).row);
}

// previous_start_of_word / next_end_of_word. Rust asks
// unicode-segmentation for the word bounds and takes the nearest one whose
// text is not all whitespace; the same answer falls out of walking the
// character classes text_boundary.rs already sorts characters into.
int InputPreviousStartOfWord(const InputState* s) {
    Str t = InputValue(s);
    int off = RopeClipOffset(t, s->selectedRange.start, Bias::Left);
    while (off > 0) {
        int prev = Utf8Prev(t, off);
        uint32_t c = 0;
        Utf8At(t, prev, &c);
        CharKind k = CharKindOf(c);
        if (k != CharKind::Whitespace && k != CharKind::Newline) {
            break;
        }
        off = prev;
    }
    if (off <= 0) {
        return 0;
    }
    uint32_t first = 0;
    Utf8At(t, Utf8Prev(t, off), &first);
    CharKind kind = CharKindOf(first);
    while (off > 0) {
        int prev = Utf8Prev(t, off);
        uint32_t c = 0;
        Utf8At(t, prev, &c);
        if (CharKindOf(c) != kind) {
            break;
        }
        off = prev;
    }
    return off;
}

int InputNextEndOfWord(const InputState* s) {
    Str t = InputValue(s);
    int off = RopeClipOffset(t, InputCursor(s), Bias::Left);
    while (off < t.len) {
        uint32_t c = 0;
        int n = Utf8At(t, off, &c);
        CharKind k = CharKindOf(c);
        if (k != CharKind::Whitespace && k != CharKind::Newline) {
            break;
        }
        off += n;
    }
    if (off >= t.len) {
        return t.len;
    }
    uint32_t first = 0;
    Utf8At(t, off, &first);
    CharKind kind = CharKindOf(first);
    while (off < t.len) {
        uint32_t c = 0;
        int n = Utf8At(t, off, &c);
        if (CharKindOf(c) != kind) {
            break;
        }
        off += n;
    }
    return off;
}

static void Notify(App* app, Window* win) {
    if (win) {
        AppInvalidate(win);
    }
    (void)app;
}

static void Emit(InputState* s, App* app, Window* win, InputEvent ev) {
    if (!s->onChange.IsValid() || !s->emitEvents) {
        return;
    }
    ListenerCall(app, win, s->onChange, &ev);
}

// pause_blink_cursor: solid while the user is doing something, so the caret
// never blinks out under their hands.
static void PauseBlink(InputState* s, App* app, Window* win) {
    if (win) {
        BlinkPause(app, win, &s->blink);
    }
}

// update_preferred_column. Rust remembers the measured x as well and falls
// back to the column; without a display map there is only the column.
static void UpdatePreferredColumn(InputState* s) {
    s->preferredColumn = RopeOffsetToPoint(InputValue(s), InputCursor(s))
                             .column;
}

// RIGHT_MARGIN: how much of the run stays visible past the caret when the
// field scrolls sideways to reach it.
static const float kInputRightMargin = 5.f;

void InputScrollToCaret(InputState* s, float caretX, float caretY,
                        InputMoveDir dir) {
    if (!s) {
        return;
    }
    float wasY = s->scrollY;
    float lineH = s->lastLineH > 0 ? s->lastLineH : kInputLineH;

    // Sideways: the caret keeps a margin from either edge of the box.
    if (s->viewW > 0) {
        if (caretX - kInputRightMargin < s->scrollX) {
            s->scrollX = caretX - kInputRightMargin;
        } else if (caretX + kInputRightMargin > s->scrollX + s->viewW) {
            s->scrollX = caretX + kInputRightMargin - s->viewW;
        }
        float mostX = s->contentW - s->viewW;
        if (s->scrollX > mostX) {
            s->scrollX = mostX;
        }
        if (s->scrollX < 0) {
            s->scrollX = 0;
        }
    }

    // Down the page: the caret's whole line has to be inside the box, with a
    // line's clearance at whichever edge it came in from.
    if (s->viewH > 0) {
        if (caretY - lineH < s->scrollY) {
            s->scrollY = caretY - lineH;
        } else if (caretY + lineH + lineH > s->scrollY + s->viewH) {
            s->scrollY = caretY + lineH + lineH - s->viewH;
        }
        // A move that went up is never answered by scrolling down.
        if (dir == InputMoveDir::Up && s->scrollY > wasY) {
            s->scrollY = wasY;
        } else if (dir == InputMoveDir::Down && s->scrollY < wasY) {
            s->scrollY = wasY;
        }
        float mostY = s->contentH - s->viewH;
        if (mostY < 0) {
            mostY = 0;
        }
        if (s->scrollY > mostY) {
            s->scrollY = mostY;
        }
        if (s->scrollY < 0) {
            s->scrollY = 0;
        }
    }
}

void InputScrollToCursor(InputState* s, InputMoveDir dir) {
    if (!s) {
        return;
    }
    float lineH = s->lastLineH > 0 ? s->lastLineH : kInputLineH;
    int row = RopeOffsetToPoint(InputValue(s), InputCursor(s)).row;
    InputScrollToCaret(s, s->caretX, (float)row * lineH, dir);
}

void InputMoveTo(InputState* s, App* app, Window* win, int offset) {
    UndoBreakCoalescing(&s->undo);
    Str t = InputValue(s);
    if (offset < 0) {
        offset = 0;
    }
    if (offset > t.len) {
        offset = t.len;
    }
    s->selectedRange = SelectionAt(offset);
    s->hasSelectedWordRange = false;
    PauseBlink(s, app, win);
    UpdatePreferredColumn(s);
    // scroll_to: the caret takes the view with it.
    InputScrollToCursor(s, InputMoveDir::None);
    Notify(app, win);
}

void InputSelectTo(InputState* s, App* app, Window* win, int offset) {
    Str t = InputValue(s);
    if (offset < 0) {
        offset = 0;
    }
    if (offset > t.len) {
        offset = t.len;
    }
    if (s->selectionReversed) {
        s->selectedRange.start = offset;
    } else {
        s->selectedRange.end = offset;
    }
    if (s->selectedRange.end < s->selectedRange.start) {
        s->selectionReversed = !s->selectionReversed;
        Selection flipped = {s->selectedRange.end, s->selectedRange.start};
        s->selectedRange = flipped;
    }
    // A double click's word stays whole: dragging out of it may only grow the
    // selection, never eat back into the word it started from.
    if (s->hasSelectedWordRange) {
        if (s->selectedRange.start > s->selectedWordRange.start) {
            s->selectedRange.start = s->selectedWordRange.start;
        }
        if (s->selectedRange.end < s->selectedWordRange.end) {
            s->selectedRange.end = s->selectedWordRange.end;
        }
    }
    if (s->selectedRange.IsEmpty()) {
        UpdatePreferredColumn(s);
    }
    Notify(app, win);
}

void InputSelectAll(InputState* s, App* app, Window* win) {
    UndoBreakCoalescing(&s->undo);
    s->selectedRange = Selection{0, InputValue(s).len};
    s->selectionReversed = false;
    s->hasSelectedWordRange = false;
    Notify(app, win);
}

void InputUnselect(InputState* s, App* app, Window* win) {
    UndoBreakCoalescing(&s->undo);
    int offset = InputCursor(s);
    s->selectedRange = SelectionAt(offset);
    s->hasSelectedWordRange = false;
    Notify(app, win);
}

void InputSetSelectedRange(InputState* s, App* app, Window* win, int a, int b) {
    Str t = InputValue(s);
    // A non-empty range grows out to character boundaries; an empty one stays
    // empty and clips to the boundary before it.
    Bias endBias = a == b ? Bias::Left : Bias::Right;
    int start = RopeClipOffset(t, a, Bias::Left);
    int end = RopeClipOffset(t, b, endBias);
    InputMoveTo(s, app, win, start);
    s->selectionReversed = false;
    s->hasSelectedWordRange = false;
    InputSelectTo(s, app, win, end);
}

// selection.rs: what a double and a triple click take.
void InputSelectWord(InputState* s, App* app, Window* win, int offset) {
    int a = 0;
    int b = 0;
    if (!TextWordRangeAt(InputValue(s), offset, &a, &b)) {
        return;
    }
    UndoBreakCoalescing(&s->undo);
    s->selectedRange = Selection{a, b};
    s->selectionReversed = false;
    s->selectedWordRange = s->selectedRange;
    s->hasSelectedWordRange = true;
    Notify(app, win);
}

void InputSelectLine(InputState* s, App* app, Window* win, int offset) {
    int a = 0;
    int b = 0;
    TextLineRangeAt(InputValue(s), offset, &a, &b);
    UndoBreakCoalescing(&s->undo);
    s->selectedRange = Selection{a, b};
    s->selectionReversed = false;
    s->hasSelectedWordRange = false;
    Notify(app, win);
}

// ─── the edit path ────────────────────────────────────────────────────────

static bool StrSameBytes(Str a, Str b) {
    if (a.len != b.len) {
        return false;
    }
    return a.len == 0 || memcmp(a.s, b.s, (size_t)a.len) == 0;
}

// is_valid_input: the validator, the mask, and (in Rust) a regex we have no
// engine for.
static bool IsValidInput(const InputState* s, Str text) {
    if (text.len == 0) {
        return true;
    }
    if (s->validate && !s->validate(text, s->validateArg)) {
        return false;
    }
    return MaskIsValid(s->maskPattern, text);
}

// normalize_input: a number mask folds full-width digits to ASCII, and a
// single-line field never takes a newline.
static Str NormalizeInput(Arena* a, const InputState* s, Str newText) {
    Str out = s->maskPattern.kind == MaskKind::Number
                  ? NormalizeNumberInput(a, newText)
                  : newText;
    if (!InputIsSingleLine(s)) {
        return out;
    }
    bool hasBreak = false;
    for (int i = 0; i < out.len && !hasBreak; i++) {
        hasBreak = out.s[i] == '\n' || out.s[i] == '\r';
    }
    if (!hasBreak) {
        return out;
    }
    char* buf = (char*)Alloc(a, (size_t)out.len + 1);
    int n = 0;
    for (int i = 0; i < out.len; i++) {
        if (out.s[i] != '\n' && out.s[i] != '\r') {
            buf[n++] = out.s[i];
        }
    }
    buf[n] = 0;
    return Str(buf, n);
}

// push_history. `oldAll` is the whole document as it was before the splice;
// `range` indexes into it.
static void PushHistory(InputState* s, Str oldAll, Selection range, Str newText,
                        bool hasIntent, EditIntent requested,
                        Selection selBefore, const Selection* selAfter) {
    if (UndoIsIgnoring(&s->undo)) {
        return;
    }
    Selection r = {RopeClipOffset(oldAll, range.start, Bias::Left),
                   RopeClipOffset(oldAll, range.end, Bias::Right)};
    if (r.end < r.start) {
        r.end = r.start;
    }
    Str oldText = Str(oldAll.s + r.start, r.end - r.start);
    Selection newRange = {r.start, r.start + newText.len};

    EditIntent intent = requested;
    if (!hasIntent) {
        bool typed = r.IsEmpty() && oldText.len == 0 && newText.len > 0;
        for (int i = 0; typed && i < newText.len; i++) {
            typed = newText.s[i] != '\n' && newText.s[i] != '\r';
        }
        intent = typed ? EditIntent::Typing : EditIntent::Atomic;
    }
    // A delete's "before" is where the caret stood, which is the far end of
    // what it removed; that is what an undo has to put back.
    Selection before = selBefore;
    if (intent == EditIntent::Backspace) {
        before = SelectionAt(r.end);
    } else if (intent == EditIntent::DeleteForward) {
        before = SelectionAt(r.start);
    }

    Change c = {};
    c.oldRange = r;
    c.oldText = StrDup(oldText);
    c.newRange = newRange;
    c.newText = StrDup(newText);
    c.selBefore = before;
    c.selAfter = selAfter ? *selAfter : SelectionAt(newRange.end);
    UndoRecordTransaction(&s->undo, c, intent);
}

bool InputReplaceTextInRange(InputState* s, App* app, Window* win,
                             const Selection* range, Str newText) {
    bool hasIntent = s->undo.hasPendingIntent;
    EditIntent requested = s->undo.pendingIntent;
    s->undo.hasPendingIntent = false;
    if (!InputIsEditable(s)) {
        return false;
    }
    Selection selBefore = s->selectedRange;
    if (win && BlinkVisible(app, s->blink)) {
        PauseBlink(s, app, win);
    }

    Arena* tmp = GetTempArena();
    Str text = NormalizeInput(tmp, s, newText);
    // A commit with no range of its own replaces whatever the input method
    // had provisionally put in, not the selection: the marked text is what
    // the candidate was standing in for.
    Selection r = range           ? *range
                  : s->imeMarking ? s->imeMarked
                                  : s->selectedRange;
    Str before = InputValue(s);
    if (r.start < 0) {
        r.start = 0;
    }
    if (r.end > before.len) {
        r.end = before.len;
    }
    if (r.end < r.start) {
        r.end = r.start;
    }
    // The document as it was, which push_history indexes and an invalid edit
    // is rolled back to.
    Str oldAll = StrDup(tmp, before);

    TextSplice(s, r.start, r.end, text);
    int newOffset = r.start + text.len;
    if (newOffset > s->text.len) {
        newOffset = s->text.len;
    }
    bool maskChanged = false;

    if (InputIsSingleLine(s)) {
        Str pending = InputValue(s);
        // Only reject the edit if the old text was valid, so a default_value
        // that does not conform cannot trap the field: the user can still edit
        // their way out of it.
        if (!IsValidInput(s, pending) && IsValidInput(s, oldAll)) {
            TextSet(s, oldAll);
            return false;
        }
        if (!MaskIsNone(s->maskPattern)) {
            Str maskText = MaskApply(tmp, s->maskPattern, pending);
            maskChanged = !StrSameBytes(maskText, pending);
            int grown = text.len + maskText.len - pending.len;
            if (grown < 0) {
                grown = 0;
            }
            TextSet(s, maskText);
            newOffset = r.start + grown;
            if (newOffset > maskText.len) {
                newOffset = maskText.len;
            }
        }
    }

    if (maskChanged) {
        // Masking rewrites the whole document, so a segment-based entry no
        // longer matches it — record a whole-document change instead, and
        // undo/redo can restore the text exactly.
        Selection after = SelectionAt(newOffset);
        PushHistory(s, oldAll, Selection{0, oldAll.len}, InputValue(s), true,
                    EditIntent::Atomic, selBefore, &after);
    } else {
        PushHistory(s, oldAll, r, text, hasIntent, requested, selBefore,
                    nullptr);
    }

    s->selectedRange = SelectionAt(newOffset);
    s->selectionReversed = false;
    s->hasSelectedWordRange = false;
    // The text went in for real, so there is nothing provisional left.
    s->imeMarking = false;
    s->imeMarked = {};
    UpdatePreferredColumn(s);
    if (InputIsMultiLine(s) && s->mode.kind == LayoutModeKind::AutoGrow) {
        LayoutModeSetRows(&s->mode, RopeLinesLen(InputValue(s)));
    }
    Emit(s, app, win, InputEvent{InputEventKind::Change});
    Notify(app, win);
    return true;
}

bool InputMarkedRange(const InputState* s, Selection* out) {
    if (!s || !s->imeMarking) {
        return false;
    }
    if (out) {
        *out = s->imeMarked;
    }
    return true;
}

void InputUnmarkText(InputState* s, App* app, Window* win) {
    if (!s || !s->imeMarking) {
        return;
    }
    s->imeMarking = false;
    s->imeMarked = {};
    // The whole composition was one transaction, so it undoes as one thing
    // rather than one candidate at a time.
    UndoCommitTransaction(&s->undo);
    Notify(app, win);
}

void InputReplaceAndMarkText(InputState* s, App* app, Window* win,
                             const Selection* range, Str newText,
                             const Selection* sel) {
    if (!s || !InputIsEditable(s)) {
        return;
    }
    bool hasIntent = s->undo.hasPendingIntent;
    EditIntent requested = s->undo.pendingIntent;
    s->undo.hasPendingIntent = false;
    Selection selBefore = s->selectedRange;
    bool startsComposition = !s->imeMarking;
    if (startsComposition) {
        UndoBeginTransaction(&s->undo);
    }
    if (win && BlinkVisible(app, s->blink)) {
        PauseBlink(s, app, win);
    }
    Arena* tmp = GetTempArena();
    Str text = NormalizeInput(tmp, s, newText);
    Selection r = range           ? *range
                  : s->imeMarking ? s->imeMarked
                                  : s->selectedRange;
    Str before = InputValue(s);
    if (r.start < 0) {
        r.start = 0;
    }
    if (r.end > before.len) {
        r.end = before.len;
    }
    if (r.end < r.start) {
        r.end = r.start;
    }
    Str oldAll = StrDup(tmp, before);
    TextSplice(s, r.start, r.end, text);
    if (InputIsSingleLine(s)) {
        // The same rule the committed path uses: only refuse the edit when it
        // is the edit that broke the field, so a value that never conformed
        // cannot trap it.
        Str pending = InputValue(s);
        if (!IsValidInput(s, pending) && IsValidInput(s, oldAll)) {
            TextSet(s, oldAll);
            if (startsComposition) {
                UndoCommitTransaction(&s->undo);
            }
            return;
        }
    }
    if (text.len == 0) {
        // An empty insert is the composition being abandoned: the caret goes
        // back where it started and nothing is marked.
        s->selectedRange = SelectionAt(r.start);
        s->imeMarking = false;
        s->imeMarked = {};
    } else {
        s->imeMarking = true;
        s->imeMarked = Selection{r.start, r.start + text.len};
        if (sel) {
            int lo = r.start + sel->start;
            int hi = r.start + sel->end;
            int end = r.start + text.len;
            s->selectedRange =
                Selection{lo < r.start ? r.start : (lo > end ? end : lo),
                          hi < r.start ? r.start : (hi > end ? end : hi)};
        } else {
            s->selectedRange = SelectionAt(r.start + text.len);
        }
    }
    s->selectionReversed = false;
    s->hasSelectedWordRange = false;
    // Every candidate is a change inside the open transaction, so an undo
    // after the composition takes the whole of it back rather than stepping
    // through the candidates one at a time.
    Selection after = s->selectedRange;
    PushHistory(s, oldAll, r, text, hasIntent, requested, selBefore, &after);
    UpdatePreferredColumn(s);
    if (InputIsMultiLine(s) && s->mode.kind == LayoutModeKind::AutoGrow) {
        LayoutModeSetRows(&s->mode, RopeLinesLen(InputValue(s)));
    }
    if (text.len == 0) {
        UndoCommitTransaction(&s->undo);
    }
    Notify(app, win);
}

// with_edits_allowed: disabled and readonly reject what the *user* does; a
// programmatic write always goes through.
struct EditsAllowed {
    InputState* s;
    bool wasDisabled;
    bool wasReadonly;

    explicit EditsAllowed(InputState* state)
        : s(state), wasDisabled(state->disabled), wasReadonly(state->readonly) {
        s->disabled = false;
        s->readonly = false;
    }
    ~EditsAllowed() {
        s->disabled = wasDisabled;
        s->readonly = wasReadonly;
    }
};

static void ReplaceText(InputState* s, App* app, Window* win, Str value) {
    EditsAllowed allow(s);
    s->undo.hasPendingIntent = true;
    s->undo.pendingIntent = EditIntent::Atomic;
    Selection all = {0, InputValue(s).len};
    InputReplaceTextInRange(s, app, win, &all, value);
}

// reset_selection: a single-line field puts the caret at the end, matching an
// HTML <input>; a multi-line one goes back to 0..0.
static void ResetSelection(InputState* s) {
    if (InputIsSingleLine(s)) {
        s->selectedRange = SelectionAt(InputValue(s).len);
    } else {
        s->selectedRange = {};
    }
    s->selectionReversed = false;
    s->hasSelectedWordRange = false;
}

// set_value(): the programmatic write, which is not an edit — it clears the
// undo history rather than becoming a step in it. Rust takes a window and a
// context because it emits and notifies; this one suppresses both, so seeding
// a field before the window exists is the same call as changing it later.
void InputSetValue(InputState* s, Str value) {
    App* app = nullptr;
    Window* win = nullptr;
    UndoSetIgnoring(&s->undo, true);
    s->emitEvents = false;
    ReplaceText(s, app, win, value);
    UndoSetIgnoring(&s->undo, false);
    s->emitEvents = true;
    ResetSelection(s);
    UndoClear(&s->undo);
    Notify(app, win);
}

void InputReplaceAll(InputState* s, App* app, Window* win, Str value) {
    ReplaceText(s, app, win, value);
    ResetSelection(s);
    Notify(app, win);
}

void InputInsert(InputState* s, App* app, Window* win, Str value) {
    EditsAllowed allow(s);
    s->undo.hasPendingIntent = true;
    s->undo.pendingIntent = EditIntent::Atomic;
    Selection at = SelectionAt(InputCursor(s));
    InputReplaceTextInRange(s, app, win, &at, value);
    s->selectedRange = SelectionAt(s->selectedRange.end);
}

void InputClean(InputState* s, App* app, Window* win) {
    ReplaceText(s, app, win, Str{});
    s->selectedRange = {};
    s->selectionReversed = false;
    Notify(app, win);
}

void InputSetPlaceholder(InputState* s, Str value) {
    StrFree(s->placeholder);
    s->placeholder = StrDup(value);
}

void InputSetMaskPattern(InputState* s, MaskPattern pattern) {
    MaskPatternFree(&s->maskPattern);
    s->maskPattern = pattern;
    s->maskPatternSet = true;
    // Rust's `mask_pattern()` builder puts the derived cue in as well.
    Str cue = MaskPlaceholder(GetTempArena(), s->maskPattern);
    if (cue.len > 0) {
        InputSetPlaceholder(s, cue);
    }
}

void InputTypeChar(InputState* s, App* app, Window* win, uint32_t ch) {
    char buf[4];
    int n = 0;
    if (ch < 0x80) {
        buf[n++] = (char)ch;
    } else if (ch < 0x800) {
        buf[n++] = (char)(0xC0 | (ch >> 6));
        buf[n++] = (char)(0x80 | (ch & 0x3F));
    } else if (ch < 0x10000) {
        buf[n++] = (char)(0xE0 | (ch >> 12));
        buf[n++] = (char)(0x80 | ((ch >> 6) & 0x3F));
        buf[n++] = (char)(0x80 | (ch & 0x3F));
    } else {
        buf[n++] = (char)(0xF0 | (ch >> 18));
        buf[n++] = (char)(0x80 | ((ch >> 12) & 0x3F));
        buf[n++] = (char)(0x80 | ((ch >> 6) & 0x3F));
        buf[n++] = (char)(0x80 | (ch & 0x3F));
    }
    InputReplaceTextInRange(s, app, win, nullptr, Str(buf, n));
    PauseBlink(s, app, win);
}

// ─── movement ─────────────────────────────────────────────────────────────

// move_vertical, on logical lines. Rust walks the display map so a soft-wrapped
// row counts as its own line; without one, a wrapped line moves as a whole.
static void MoveVertical(InputState* s, App* app, Window* win, int lines) {
    if (InputIsSingleLine(s)) {
        return;
    }
    Str t = InputValue(s);
    RopePoint p = RopeOffsetToPoint(t, InputCursor(s));
    int column = s->preferredColumn >= 0 ? s->preferredColumn : p.column;
    int maxRow = RopeLinesLen(t) - 1;
    int row = p.row + lines;
    if (row < 0) {
        row = 0;
    }
    if (row > maxRow) {
        row = maxRow;
    }
    int lineLen = RopeLineLen(t, row);
    int want = column < lineLen ? column : lineLen;
    int offset =
        RopeClipOffset(t, RopeLineStartOffset(t, row) + want, Bias::Left);
    PauseBlink(s, app, win);
    InputMoveTo(s, app, win, offset);
    // move_to recomputed it from where the caret landed; the whole point is to
    // keep aiming at the column the run started from.
    s->preferredColumn = column;
}

static void SelectVertical(InputState* s, App* app, Window* win, int lines) {
    if (InputIsSingleLine(s)) {
        return;
    }
    UndoBreakCoalescing(&s->undo);
    Str t = InputValue(s);
    if (lines < 0) {
        int offset = InputStartOfLine(s);
        InputSelectTo(s, app, win, InputPreviousBoundary(s, offset));
    } else {
        int offset = InputEndOfLine(s) + 1;
        if (offset > t.len) {
            offset = t.len;
        }
        InputSelectTo(s, app, win, InputNextBoundary(s, offset));
    }
}

// ─── actions ──────────────────────────────────────────────────────────────

static void DeleteRange(InputState* s, App* app, Window* win, int a, int b) {
    if (a > b) {
        int t = a;
        a = b;
        b = t;
    }
    Selection r = {a, b};
    InputReplaceTextInRange(s, app, win, &r, Str{});
    PauseBlink(s, app, win);
}

static void DoCopy(InputState* s, Window* win) {
    if (s->selectedRange.IsEmpty() || !win) {
        return;
    }
    ClipboardSetText(win, InputSelectedValue(s));
}

static void DoUndo(InputState* s, App* app, Window* win) {
    UndoSetIgnoring(&s->undo, true);
    const UndoTransaction* t = UndoPopUndo(&s->undo);
    if (t && t->len > 0) {
        // The list is applied backwards, so the selection to restore is the
        // one recorded before the first change in it.
        Selection sel = t->changes[0].selBefore;
        for (int i = t->len - 1; i >= 0; i--) {
            Selection r = t->changes[i].newRange;
            InputReplaceTextInRange(s, app, win, &r, t->changes[i].oldText);
        }
        s->selectedRange = sel;
        s->selectionReversed = false;
    }
    UndoSetIgnoring(&s->undo, false);
}

static void DoRedo(InputState* s, App* app, Window* win) {
    UndoSetIgnoring(&s->undo, true);
    const UndoTransaction* t = UndoPopRedo(&s->undo);
    if (t && t->len > 0) {
        Selection sel = t->changes[t->len - 1].selAfter;
        for (int i = 0; i < t->len; i++) {
            Selection r = t->changes[i].oldRange;
            InputReplaceTextInRange(s, app, win, &r, t->changes[i].newText);
        }
        s->selectedRange = sel;
        s->selectionReversed = false;
    }
    UndoSetIgnoring(&s->undo, false);
}

bool InputPerform(InputState* s, App* app, Window* win, InputAction action,
                  bool shift) {
    if (!s) {
        return false;
    }
    Str t = InputValue(s);
    switch (action) {
        case InputAction::None:
            return false;

        case InputAction::MoveLeft:
            PauseBlink(s, app, win);
            InputMoveTo(s, app, win,
                        s->selectedRange.IsEmpty()
                            ? InputPreviousBoundary(s, InputCursor(s))
                            : s->selectedRange.start);
            return true;
        case InputAction::MoveRight:
            PauseBlink(s, app, win);
            InputMoveTo(s, app, win,
                        s->selectedRange.IsEmpty()
                            ? InputNextBoundary(s, s->selectedRange.end)
                            : s->selectedRange.end);
            return true;
        case InputAction::MoveUp:
            if (InputIsSingleLine(s)) {
                return false;
            }
            if (!s->selectedRange.IsEmpty()) {
                InputMoveTo(s, app, win, s->selectedRange.start);
            }
            MoveVertical(s, app, win, -1);
            return true;
        case InputAction::MoveDown:
            if (InputIsSingleLine(s)) {
                return false;
            }
            if (!s->selectedRange.IsEmpty()) {
                InputMoveTo(s, app, win, s->selectedRange.end);
            }
            MoveVertical(s, app, win, 1);
            return true;
        case InputAction::MovePageUp:
            MoveVertical(s, app, win, -LayoutModeRows(s->mode));
            return InputIsMultiLine(s);
        case InputAction::MovePageDown:
            MoveVertical(s, app, win, LayoutModeRows(s->mode));
            return InputIsMultiLine(s);
        case InputAction::MoveHome:
            PauseBlink(s, app, win);
            InputMoveTo(s, app, win, InputStartOfLine(s));
            return true;
        case InputAction::MoveEnd:
            PauseBlink(s, app, win);
            InputMoveTo(s, app, win, InputEndOfLine(s));
            return true;
        case InputAction::MoveToStart:
            InputMoveTo(s, app, win, 0);
            return true;
        case InputAction::MoveToEnd:
            InputMoveTo(s, app, win, t.len);
            return true;
        case InputAction::MoveToPreviousWord:
            InputMoveTo(s, app, win, InputPreviousStartOfWord(s));
            return true;
        case InputAction::MoveToNextWord:
            InputMoveTo(s, app, win, InputNextEndOfWord(s));
            return true;

        case InputAction::SelectLeft:
            UndoBreakCoalescing(&s->undo);
            InputSelectTo(s, app, win,
                          InputPreviousBoundary(s, InputCursor(s)));
            return true;
        case InputAction::SelectRight:
            UndoBreakCoalescing(&s->undo);
            InputSelectTo(s, app, win, InputNextBoundary(s, InputCursor(s)));
            return true;
        case InputAction::SelectUp:
            SelectVertical(s, app, win, -1);
            return InputIsMultiLine(s);
        case InputAction::SelectDown:
            SelectVertical(s, app, win, 1);
            return InputIsMultiLine(s);
        case InputAction::SelectAll:
            InputSelectAll(s, app, win);
            return true;
        case InputAction::SelectToStart:
            UndoBreakCoalescing(&s->undo);
            InputSelectTo(s, app, win, 0);
            return true;
        case InputAction::SelectToEnd:
            UndoBreakCoalescing(&s->undo);
            InputSelectTo(s, app, win, t.len);
            return true;
        case InputAction::SelectToStartOfLine:
            UndoBreakCoalescing(&s->undo);
            InputSelectTo(s, app, win, InputStartOfLine(s));
            return true;
        case InputAction::SelectToEndOfLine:
            UndoBreakCoalescing(&s->undo);
            InputSelectTo(s, app, win, InputEndOfLine(s));
            return true;
        case InputAction::SelectToPreviousWordStart:
            UndoBreakCoalescing(&s->undo);
            InputSelectTo(s, app, win, InputPreviousStartOfWord(s));
            return true;
        case InputAction::SelectToNextWordEnd:
            UndoBreakCoalescing(&s->undo);
            InputSelectTo(s, app, win, InputNextEndOfWord(s));
            return true;

        case InputAction::Backspace: {
            EditIntent intent = EditIntent::Atomic;
            if (s->selectedRange.IsEmpty()) {
                InputSelectTo(s, app, win,
                              InputPreviousBoundary(s, InputCursor(s)));
                intent = EditIntent::Backspace;
            }
            s->undo.hasPendingIntent = true;
            s->undo.pendingIntent = intent;
            InputReplaceTextInRange(s, app, win, nullptr, Str{});
            PauseBlink(s, app, win);
            return true;
        }
        case InputAction::Delete: {
            EditIntent intent = EditIntent::Atomic;
            if (s->selectedRange.IsEmpty()) {
                InputSelectTo(s, app, win,
                              InputNextBoundary(s, InputCursor(s)));
                intent = EditIntent::DeleteForward;
            }
            s->undo.hasPendingIntent = true;
            s->undo.pendingIntent = intent;
            InputReplaceTextInRange(s, app, win, nullptr, Str{});
            PauseBlink(s, app, win);
            return true;
        }
        case InputAction::DeleteToBeginningOfLine: {
            if (!s->selectedRange.IsEmpty()) {
                InputReplaceTextInRange(s, app, win, nullptr, Str{});
                PauseBlink(s, app, win);
                return true;
            }
            int offset = InputStartOfLine(s);
            if (offset == InputCursor(s) && offset > 0) {
                offset--;
            }
            DeleteRange(s, app, win, offset, InputCursor(s));
            return true;
        }
        case InputAction::DeleteToEndOfLine: {
            if (!s->selectedRange.IsEmpty()) {
                InputReplaceTextInRange(s, app, win, nullptr, Str{});
                PauseBlink(s, app, win);
                return true;
            }
            int offset = InputEndOfLine(s);
            if (offset == InputCursor(s)) {
                offset = offset + 1 > t.len ? t.len : offset + 1;
            }
            DeleteRange(s, app, win, InputCursor(s), offset);
            return true;
        }
        case InputAction::DeleteToPreviousWordStart: {
            if (!s->selectedRange.IsEmpty()) {
                InputReplaceTextInRange(s, app, win, nullptr, Str{});
                PauseBlink(s, app, win);
                return true;
            }
            DeleteRange(s, app, win, InputPreviousStartOfWord(s),
                        InputCursor(s));
            return true;
        }
        case InputAction::DeleteToNextWordEnd: {
            if (!s->selectedRange.IsEmpty()) {
                InputReplaceTextInRange(s, app, win, nullptr, Str{});
                PauseBlink(s, app, win);
                return true;
            }
            DeleteRange(s, app, win, InputCursor(s), InputNextEndOfWord(s));
            return true;
        }

        case InputAction::Enter: {
            // A multi-line input takes a newline, unless it submits on Enter —
            // then only Shift+Enter does, and a plain Enter is the submit.
            bool insertNewline =
                InputIsMultiLine(s) && (!s->submitOnEnter || shift);
            bool handled = false;
            if (insertNewline) {
                InputReplaceTextInRange(s, app, win, nullptr, StrL("\n"));
                PauseBlink(s, app, win);
                handled = true;
            } else {
                UndoBreakCoalescing(&s->undo);
            }
            InputEvent ev = {};
            ev.kind = InputEventKind::PressEnter;
            ev.shift = shift;
            Emit(s, app, win, ev);
            return handled;
        }
        case InputAction::Escape:
            if (s->cleanOnEscape) {
                InputClean(s, app, win);
                return true;
            }
            return false;

        case InputAction::Copy:
            DoCopy(s, win);
            return true;
        case InputAction::Cut:
            if (s->selectedRange.IsEmpty()) {
                return true;
            }
            DoCopy(s, win);
            s->undo.hasPendingIntent = true;
            s->undo.pendingIntent = EditIntent::Atomic;
            InputReplaceTextInRange(s, app, win, nullptr, Str{});
            return true;
        case InputAction::Paste: {
            if (!win) {
                return true;
            }
            Str text = ClipboardGetText(GetTempArena(), win);
            if (text.len == 0) {
                return true;
            }
            s->undo.hasPendingIntent = true;
            s->undo.pendingIntent = EditIntent::Atomic;
            InputReplaceTextInRange(s, app, win, nullptr, text);
            return true;
        }
        case InputAction::Undo:
            DoUndo(s, app, win);
            Notify(app, win);
            return true;
        case InputAction::Redo:
            DoRedo(s, app, win);
            Notify(app, win);
            return true;
    }
    return false;
}

// The keymap state.rs::init installs, folded into one function. GPUI resolves
// a chord against a bound action list; there is one input context here, so a
// switch says the same thing. The `cmd-` bindings are the macOS spelling of
// the `ctrl-` ones below them and land on the same actions.
InputAction InputActionForKey(const InputState* s, int vk, bool shift,
                              bool ctrl, bool alt) {
    (void)s;
    bool word = ctrl || alt; // ctrl- off macOS, alt- on it
    switch (vk) {
        case KeyLeft:
            if (word) {
                return shift ? InputAction::SelectToPreviousWordStart
                             : InputAction::MoveToPreviousWord;
            }
            return shift ? InputAction::SelectLeft : InputAction::MoveLeft;
        case KeyRight:
            if (word) {
                return shift ? InputAction::SelectToNextWordEnd
                             : InputAction::MoveToNextWord;
            }
            return shift ? InputAction::SelectRight : InputAction::MoveRight;
        case KeyUp:
            return shift ? InputAction::SelectUp : InputAction::MoveUp;
        case KeyDown:
            return shift ? InputAction::SelectDown : InputAction::MoveDown;
        case KeyPageUp:
            return InputAction::MovePageUp;
        case KeyPageDown:
            return InputAction::MovePageDown;
        case KeyHome:
            // Rust spells the document ends cmd-up / cmd-down, which is what
            // ctrl-home / ctrl-end is everywhere else.
            if (ctrl) {
                return shift ? InputAction::SelectToStart
                             : InputAction::MoveToStart;
            }
            return shift ? InputAction::SelectToStartOfLine
                         : InputAction::MoveHome;
        case KeyEnd:
            if (ctrl) {
                return shift ? InputAction::SelectToEnd
                             : InputAction::MoveToEnd;
            }
            return shift ? InputAction::SelectToEndOfLine
                         : InputAction::MoveEnd;
        case KeyBack:
            return word ? InputAction::DeleteToPreviousWordStart
                        : InputAction::Backspace;
        case KeyDelete:
            return word ? InputAction::DeleteToNextWordEnd
                        : InputAction::Delete;
        case KeyReturn:
            return InputAction::Enter;
        case KeyEscape:
            return InputAction::Escape;
        case KeyA:
            if (ctrl) {
                return shift ? InputAction::SelectToStartOfLine
                             : InputAction::SelectAll;
            }
            return InputAction::None;
        case KeyC:
            return ctrl ? InputAction::Copy : InputAction::None;
        case KeyX:
            return ctrl ? InputAction::Cut : InputAction::None;
        case KeyV:
            return ctrl ? InputAction::Paste : InputAction::None;
        case KeyZ:
            if (!ctrl) {
                return InputAction::None;
            }
            return shift ? InputAction::Redo : InputAction::Undo;
        case KeyY:
            return ctrl ? InputAction::Redo : InputAction::None;
        case KeyE:
            if (ctrl) {
                return shift ? InputAction::SelectToEndOfLine
                             : InputAction::MoveEnd;
            }
            return InputAction::None;
        default:
            return InputAction::None;
    }
}

// ─── focus ────────────────────────────────────────────────────────────────

void InputFocus(InputState* s, App* app, Window* win) {
    if (!s || !win) {
        return;
    }
    if (win->input && win->input != s) {
        InputBlur(win->input, app, win);
    }
    s->focused = true;
    win->input = s;
    win->prevInput = s;
    BlinkStart(app, win, &s->blink);
    Emit(s, app, win, InputEvent{InputEventKind::Focus});
    Notify(app, win);
}

void InputBlur(InputState* s, App* app, Window* win) {
    if (!s) {
        return;
    }
    // Blurring ends the typing session, so a later undo stops here rather than
    // swallowing everything typed before the field lost focus.
    UndoBreakCoalescing(&s->undo);
    s->focused = false;
    s->selecting = false;
    if (win) {
        BlinkStop(app, win, &s->blink);
        if (win->input == s) {
            win->input = nullptr;
            win->prevInput = nullptr;
        }
    }
    Emit(s, app, win, InputEvent{InputEventKind::Blur});
    Notify(app, win);
}

// index_for_mouse_position. The element recorded the run it painted, so the
// press is measured against that rather than against the whole field. Rust
// asks the display map which visible row the y landed on and then the shaped
// line for the x; the rows here are the logical lines, evenly spaced from the
// first one, so the row is arithmetic and only the x needs shaping.
int InputIndexForPosition(const InputState* s, PaintCtx* ctx, float x,
                          float y) {
    Str t = InputValue(s);
    if (t.len == 0 || !ctx) {
        return 0;
    }
    const Bounds& b = s->lastBounds;
    if (b.w <= 0 && b.h <= 0) {
        return 0;
    }
    float font = s->lastFont > 0 ? s->lastFont : 14.f;
    if (InputIsSingleLine(s)) {
        if (x <= b.x) {
            return 0;
        }
        return TextIndexAt(ctx, t, font, 0, false, x - b.x, 0);
    }
    float lineH = s->lastLineH > 0 ? s->lastLineH : b.h;
    int rows = RopeLinesLen(t);
    int row = lineH > 0 ? (int)((y - b.y) / lineH) : 0;
    if (row < 0) {
        row = 0;
    }
    if (row > rows - 1) {
        row = rows - 1;
    }
    Str line = RopeSliceLine(t, row);
    int start = RopeLineStartOffset(t, row);
    if (line.len == 0 || x <= b.x) {
        return start;
    }
    return start + TextIndexAt(ctx, line, font, s->softWrap ? b.w : 0,
                               s->softWrap, x - b.x, 0);
}

/* Port of crates/base/src/input/base/mask_pattern.rs.

   Rust parses the pattern once into a `Vec<MaskToken>` and keeps it beside the
   pattern string. A token is a pure function of its character, so here the
   pattern string is the whole state and MaskTokenAt reads it — the patterns
   are a dozen characters long and every walk over them is already a walk over
   the text beside it.

   Rust indexes both the pattern and the text by *character*, not by byte, so
   everything below steps codepoints. */

static bool IsAsciiDigit(uint32_t c) {
    return c >= '0' && c <= '9';
}
static bool IsAsciiAlpha(uint32_t c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
static bool IsAsciiAlnum(uint32_t c) {
    return IsAsciiDigit(c) || IsAsciiAlpha(c);
}
static bool IsSign(uint32_t c) {
    return c == '+' || c == '-';
}

// MaskToken::is_match. A separator matches only itself.
static bool TokenIsMatch(MaskToken tok, uint32_t sep, uint32_t ch) {
    switch (tok) {
        case MaskToken::Digit:
            return IsAsciiDigit(ch);
        case MaskToken::Letter:
            return IsAsciiAlpha(ch);
        case MaskToken::LetterOrDigit:
            return IsAsciiAlnum(ch);
        case MaskToken::Any:
            return true;
        case MaskToken::Sep:
            return sep == ch;
    }
    return false;
}

// MaskToken::mask_char.
static uint32_t TokenMaskChar(MaskToken tok, uint32_t sep, uint32_t ch) {
    return tok == MaskToken::Sep ? sep : ch;
}

// MaskToken::unmask_char. A separator contributes nothing — Rust's `None`.
static bool TokenUnmaskChar(MaskToken tok) {
    return tok != MaskToken::Sep;
}

static MaskToken TokenOf(uint32_t ch, uint32_t* sep) {
    *sep = 0;
    switch (ch) {
        case '9':
            return MaskToken::Digit;
        case 'A':
            return MaskToken::Letter;
        case '#':
            return MaskToken::LetterOrDigit;
        case '*':
            return MaskToken::Any;
        default:
            *sep = ch;
            return MaskToken::Sep;
    }
}

MaskPattern MaskPatternNew(Str pattern) {
    MaskPattern p = {};
    p.kind = MaskKind::Pattern;
    p.pattern = StrDup(pattern);
    return p;
}

MaskPattern MaskPatternNumber(uint32_t separator) {
    MaskPattern p = {};
    p.kind = MaskKind::Number;
    p.separator = separator;
    p.fraction = -1;
    return p;
}

void MaskPatternFree(MaskPattern* p) {
    if (!p) {
        return;
    }
    StrFree(p->pattern);
    p->pattern = {};
    p->kind = MaskKind::None;
}

bool MaskTokenAt(const MaskPattern& p, int pos, MaskToken* out, uint32_t* sep) {
    *out = MaskToken::Any;
    *sep = 0;
    if (p.kind != MaskKind::Pattern || pos < 0) {
        return false;
    }
    int i = RopeCharIndexToOffset(p.pattern, pos);
    uint32_t ch = 0;
    if (RopeCharAt(p.pattern, i, &ch) == 0) {
        return false;
    }
    *out = TokenOf(ch, sep);
    return true;
}

bool MaskIsNone(const MaskPattern& p) {
    switch (p.kind) {
        case MaskKind::Pattern:
            return p.pattern.len == 0;
        case MaskKind::Number:
            return false;
        case MaskKind::None:
            return true;
    }
    return true;
}

// The number half of is_valid: at most one dot, at most one sign and only at
// the front, digits or the group separator everywhere else.
static bool NumberIsValid(const MaskPattern& p, Str text) {
    if (text.len == 0) {
        return true;
    }
    int dot = -1;
    for (int i = 0; i < text.len; i++) {
        if (text.s[i] != '.') {
            continue;
        }
        if (dot >= 0) {
            return false; // only one dot is valid
        }
        dot = i;
    }
    int intEnd = dot < 0 ? text.len : dot;
    int charPos = 0;
    for (int i = 0; i < intEnd;) {
        uint32_t c = 0;
        i += Utf8At(text, i, &c);
        if (IsSign(c)) {
            // Only one sign, and only at the beginning of the string.
            if (charPos != 0) {
                return false;
            }
        } else if (!IsAsciiDigit(c) && !(p.separator && c == p.separator)) {
            return false;
        }
        charPos++;
    }
    for (int i = intEnd + 1; i < text.len && dot >= 0;) {
        uint32_t c = 0;
        i += Utf8At(text, i, &c);
        if (!IsAsciiDigit(c) && !(p.separator && c == p.separator)) {
            return false;
        }
    }
    return true;
}

bool MaskIsValid(const MaskPattern& p, Str maskText) {
    if (MaskIsNone(p)) {
        return true;
    }
    if (p.kind == MaskKind::Number) {
        return NumberIsValid(p, maskText);
    }
    // Rust walks the tokens, consuming a text character for each one that
    // matches, and calls the text valid when every character was consumed.
    int ti = 0;
    int tokens = RopeOffsetToCharIndex(p.pattern, p.pattern.len);
    for (int pos = 0; pos < tokens; pos++) {
        if (ti >= maskText.len) {
            break;
        }
        MaskToken tok = MaskToken::Any;
        uint32_t sep = 0;
        MaskTokenAt(p, pos, &tok, &sep);
        uint32_t ch = 0;
        int n = Utf8At(maskText, ti, &ch);
        if (TokenIsMatch(tok, sep, ch)) {
            ti += n;
        }
    }
    return ti == maskText.len;
}

bool MaskIsValidAt(const MaskPattern& p, uint32_t ch, int pos) {
    if (MaskIsNone(p) || p.kind != MaskKind::Pattern) {
        return true;
    }
    MaskToken tok = MaskToken::Any;
    uint32_t sep = 0;
    if (!MaskTokenAt(p, pos, &tok, &sep)) {
        return false;
    }
    if (TokenIsMatch(tok, sep, ch)) {
        return true;
    }
    // A separator is skipped over: if the token after it takes the character,
    // typing it here is valid and the separator fills itself in.
    if (tok == MaskToken::Sep) {
        MaskToken next = MaskToken::Any;
        uint32_t nextSep = 0;
        if (MaskTokenAt(p, pos + 1, &next, &nextSep) &&
            TokenIsMatch(next, nextSep, ch)) {
            return true;
        }
    }
    return false;
}

// Append one codepoint as UTF-8.
static void PushChar(StrBuilder& sb, uint32_t c) {
    if (c < 0x80) {
        sb.AppendChar((char)c);
    } else if (c < 0x800) {
        sb.AppendChar((char)(0xC0 | (c >> 6)));
        sb.AppendChar((char)(0x80 | (c & 0x3F)));
    } else if (c < 0x10000) {
        sb.AppendChar((char)(0xE0 | (c >> 12)));
        sb.AppendChar((char)(0x80 | ((c >> 6) & 0x3F)));
        sb.AppendChar((char)(0x80 | (c & 0x3F)));
    } else {
        sb.AppendChar((char)(0xF0 | (c >> 18)));
        sb.AppendChar((char)(0x80 | ((c >> 12) & 0x3F)));
        sb.AppendChar((char)(0x80 | ((c >> 6) & 0x3F)));
        sb.AppendChar((char)(0x80 | (c & 0x3F)));
    }
}

// The Number arm of mask(): regroup the integer part in threes, keep at most
// `fraction` decimals, and put the sign back on the front.
static Str MaskNumber(Arena* a, const MaskPattern& p, Str text) {
    if (!p.separator) {
        return StrDup(a, text);
    }
    // Remove the existing group separator, then split on the dot.
    StrBuilder bare;
    int dot = -1;
    for (int i = 0; i < text.len;) {
        uint32_t c = 0;
        int n = Utf8At(text, i, &c);
        if (c != p.separator) {
            if (c == '.' && dot < 0) {
                dot = bare.len;
            }
            for (int k = 0; k < n; k++) {
                bare.AppendChar(text.s[i + k]);
            }
        }
        i += n;
    }
    Str flat = Str(bare.els, bare.len);
    int intEnd = dot < 0 ? flat.len : dot;

    // Reverse the integer part for easier grouping, taking the sign out first
    // so the result cannot come out as `-,123`.
    uint32_t sign = 0;
    StrBuilder digits;
    for (int i = intEnd - 1; i >= 0; i--) {
        char c = flat.s[i];
        if (IsSign((uint32_t)(unsigned char)c) && !sign) {
            sign = (uint32_t)(unsigned char)c;
            continue;
        }
        digits.AppendChar(c);
    }
    StrBuilder grouped;
    for (int i = 0; i < digits.len; i++) {
        if (i > 0 && i % 3 == 0) {
            PushChar(grouped, p.separator);
        }
        grouped.AppendChar(digits.els[i]);
    }
    StrBuilder out;
    if (sign) {
        PushChar(out, sign);
    }
    for (int i = grouped.len - 1; i >= 0; i--) {
        out.AppendChar(grouped.els[i]);
    }
    if (dot >= 0 && p.fraction != 0) {
        out.AppendChar('.');
        int kept = 0;
        for (int i = intEnd + 1; i < flat.len;) {
            uint32_t c = 0;
            int n = Utf8At(flat, i, &c);
            if (p.fraction >= 0 && kept >= p.fraction) {
                break;
            }
            PushChar(out, c);
            kept++;
            i += n;
        }
    }
    return StrDup(a, Str(out.els, out.len));
}

Str MaskApply(Arena* a, const MaskPattern& p, Str text) {
    if (MaskIsNone(p)) {
        return StrDup(a, text);
    }
    if (p.kind == MaskKind::Number) {
        return MaskNumber(a, p, text);
    }
    StrBuilder out;
    int ti = 0;
    int tokens = RopeOffsetToCharIndex(p.pattern, p.pattern.len);
    for (int pos = 0; pos < tokens; pos++) {
        if (ti >= text.len) {
            break;
        }
        MaskToken tok = MaskToken::Any;
        uint32_t sep = 0;
        MaskTokenAt(p, pos, &tok, &sep);
        uint32_t ch = 0;
        int n = Utf8At(text, ti, &ch);
        // Break if the expected character does not match.
        if (tok != MaskToken::Sep && !MaskIsValidAt(p, ch, pos)) {
            break;
        }
        uint32_t masked = TokenMaskChar(tok, sep, ch);
        PushChar(out, masked);
        // A separator the text did not supply is filled in without consuming
        // anything, so the next token sees the same character.
        if (ch == masked) {
            ti += n;
        }
    }
    return StrDup(a, Str(out.els, out.len));
}

Str MaskUnapply(Arena* a, const MaskPattern& p, Str maskText) {
    if (p.kind == MaskKind::Number) {
        if (!p.separator) {
            return StrDup(a, maskText);
        }
        StrBuilder out;
        bool hasDot = false;
        for (int i = 0; i < maskText.len;) {
            uint32_t c = 0;
            int n = Utf8At(maskText, i, &c);
            if (c != p.separator) {
                PushChar(out, c);
                hasDot = hasDot || c == '.';
            }
            i += n;
        }
        int len = out.len;
        if (hasDot) {
            while (len > 0 && out.els[len - 1] == '0') {
                len--;
            }
        }
        return StrDup(a, Str(out.els, len));
    }
    if (p.kind == MaskKind::None) {
        return StrDup(a, maskText);
    }
    // Pattern: Rust walks the tokens against the *character* at the same
    // index, so a separator drops out and everything else is kept.
    StrBuilder out;
    int tokens = RopeOffsetToCharIndex(p.pattern, p.pattern.len);
    int ti = 0;
    for (int pos = 0; pos < tokens; pos++) {
        uint32_t ch = 0;
        int n = RopeCharAt(maskText, ti, &ch);
        if (n == 0) {
            break;
        }
        MaskToken tok = MaskToken::Any;
        uint32_t sep = 0;
        MaskTokenAt(p, pos, &tok, &sep);
        if (TokenUnmaskChar(tok)) {
            PushChar(out, ch);
        }
        ti += n;
    }
    return StrDup(a, Str(out.els, out.len));
}

Str MaskPlaceholder(Arena* a, const MaskPattern& p) {
    if (p.kind != MaskKind::Pattern) {
        return {};
    }
    StrBuilder out;
    int tokens = RopeOffsetToCharIndex(p.pattern, p.pattern.len);
    for (int pos = 0; pos < tokens; pos++) {
        MaskToken tok = MaskToken::Any;
        uint32_t sep = 0;
        MaskTokenAt(p, pos, &tok, &sep);
        // MaskToken::placeholder: a separator shows itself, everything else an
        // underscore.
        PushChar(out, tok == MaskToken::Sep ? sep : (uint32_t)'_');
    }
    return StrDup(a, Str(out.els, out.len));
}

// Every mapping is one character to one character with the same UTF-16 length,
// so IME marked-range offsets stay valid across it; the UTF-8 byte length may
// shrink from 3 to 1, which is why the caller must go on using the normalized
// string for its byte offsets.
static uint32_t NormalizeChar(uint32_t ch) {
    if (ch >= 0xFF10 && ch <= 0xFF19) { // full-width digits 0-9
        return ch - 0xFF10 + '0';
    }
    switch (ch) {
        case 0xFF0B: // ＋
            return '+';
        case 0xFF0D: // －
        case 0x2212: // −
            return '-';
        case 0xFF0E: // ．
        case 0x3002: // 。
            return '.';
        case 0xFF0C: // ，
            return ',';
        default:
            return ch;
    }
}

Str NormalizeNumberInput(Arena* a, Str text) {
    bool any = false;
    for (int i = 0; i < text.len && !any;) {
        uint32_t c = 0;
        i += Utf8At(text, i, &c);
        any = NormalizeChar(c) != c;
    }
    if (!any) {
        return StrDup(a, text); // Rust's Cow::Borrowed
    }
    StrBuilder out;
    for (int i = 0; i < text.len;) {
        uint32_t c = 0;
        i += Utf8At(text, i, &c);
        PushChar(out, NormalizeChar(c));
    }
    return StrDup(a, Str(out.els, out.len));
}

/* Port of crates/base/src/input/base/rope_ext.rs.

   Rust implements `RopeExt` for `ropey::Rope`, whose own API is char-indexed;
   every method there converts to and from byte offsets around a char index.
   The document here is a flat UTF-8 `Str`, so a byte offset is the native
   unit and the conversions run the other way — `char_index_to_offset` and
   `offset_to_char_index` are the two that still have to walk.

   Lines are split on LF alone (`LineType::LF`), so a CRLF document keeps the
   CR at the end of the line: `slice_line` on "World\r\n" is "World\r", and
   `line_end_offset` points at the LF. `word_range` and `word_at` are not
   here — they belong to the language-server hover path, and the word range a
   double click uses is text_boundary.rs's, which is TextWordRangeAt. */

int RopeClipOffset(Str text, int offset, Bias bias) {
    if (offset <= 0 || !text.s) {
        return 0;
    }
    if (offset >= text.len) {
        return text.len;
    }
    if (bias == Bias::Left) {
        return Utf8ClipLeft(text, offset);
    }
    // Bias::Right: forward to the next boundary instead.
    while (offset < text.len && ((uint8_t)text.s[offset] & 0xC0) == 0x80) {
        offset++;
    }
    return offset;
}

int RopeCharAt(Str text, int offset, uint32_t* out) {
    *out = 0;
    if (!text.s || offset < 0 || offset >= text.len) {
        return 0;
    }
    return Utf8At(text, offset, out);
}

int RopeLinesLen(Str text) {
    // len_lines(LineType::LF): one more than the number of LFs, and an empty
    // rope still has one line.
    int n = 1;
    for (int i = 0; i < text.len; i++) {
        if (text.s[i] == '\n') {
            n++;
        }
    }
    return n;
}

int RopeLineStartOffset(Str text, int row) {
    // point_to_offset(Point::new(row, 0)): a row past the end is the end.
    if (row <= 0) {
        return 0;
    }
    int seen = 0;
    for (int i = 0; i < text.len; i++) {
        if (text.s[i] != '\n') {
            continue;
        }
        seen++;
        if (seen == row) {
            return i + 1;
        }
    }
    return text.len;
}

Str RopeSliceLine(Str text, int row) {
    if (row < 0 || row >= RopeLinesLen(text)) {
        return {};
    }
    int a = RopeLineStartOffset(text, row);
    int b = a;
    while (b < text.len && text.s[b] != '\n') {
        b++;
    }
    return Str(text.s + a, b - a);
}

int RopeLineLen(Str text, int row) {
    return RopeSliceLine(text, row).len;
}

int RopeLineEndOffset(Str text, int row) {
    return RopeLineStartOffset(text, row) + RopeLineLen(text, row);
}

RopePoint RopeOffsetToPoint(Str text, int offset) {
    offset = RopeClipOffset(text, offset, Bias::Left);
    RopePoint p = {};
    int lineStart = 0;
    for (int i = 0; i < offset; i++) {
        if (text.s[i] == '\n') {
            p.row++;
            lineStart = i + 1;
        }
    }
    p.column = offset - lineStart;
    return p;
}

int RopePointToOffset(Str text, RopePoint point) {
    // Rust does not clamp the column: the callers hand it one they measured
    // off a line, and a column past the end is their bug, not this one's.
    if (point.row < 0 || point.row >= RopeLinesLen(text)) {
        return text.len;
    }
    return RopeLineStartOffset(text, point.row) + point.column;
}

// The two UTF-16 conversions the IME and every `*_utf16` range in state.rs go
// through. A character outside the BMP is one UTF-16 surrogate pair, so it
// counts as two.
int RopeOffsetToOffsetUtf16(Str text, int offset) {
    if (offset > text.len) {
        offset = text.len;
    }
    int n = 0;
    int i = 0;
    while (i < offset) {
        uint32_t c = 0;
        i += Utf8At(text, i, &c);
        n += c >= 0x10000 ? 2 : 1;
    }
    return n;
}

int RopeOffsetUtf16ToOffset(Str text, int offsetUtf16) {
    int n = 0;
    int i = 0;
    while (i < text.len && n < offsetUtf16) {
        uint32_t c = 0;
        int len = Utf8At(text, i, &c);
        n += c >= 0x10000 ? 2 : 1;
        i += len;
    }
    return i;
}

int RopeCharIndexToOffset(Str text, int charIndex) {
    int i = 0;
    int n = 0;
    while (i < text.len && n < charIndex) {
        uint32_t c = 0;
        i += Utf8At(text, i, &c);
        n++;
    }
    return i;
}

int RopeOffsetToCharIndex(Str text, int offset) {
    // Clips right, so an offset landing inside a character counts that whole
    // character.
    offset = RopeClipOffset(text, offset, Bias::Right);
    int i = 0;
    int n = 0;
    while (i < offset) {
        uint32_t c = 0;
        i += Utf8At(text, i, &c);
        n++;
    }
    return n;
}

/* Port of crates/base/src/input/base/undo_manager.rs and change.rs.

   Each edit first makes a transaction. Compatible adjacent transactions then
   coalesce until an explicit boundary — a cursor move, a paste, a blur — so a
   run of typing undoes as one step rather than a character at a time. A caller
   that performs one logical edit through several callbacks (IME composition)
   brackets them with UndoBeginTransaction / UndoCommitTransaction.

   Rust clones changes in and out of the stacks; ownership is explicit here, so
   a Change moves and the stack that holds it frees its two strings. */

static const int kMaxUndoTransactions = 1000;
static const int kMaxChangesPerTransaction = 1000;

static void ChangeFree(Change* c) {
    StrFree(c->oldText);
    StrFree(c->newText);
    c->oldText = {};
    c->newText = {};
}

static void TransactionFree(UndoTransaction* t) {
    for (int i = 0; i < t->len; i++) {
        ChangeFree(&t->changes[i]);
    }
    free(t->changes);
    t->changes = nullptr;
    t->len = 0;
    t->cap = 0;
}

static void TransactionPush(UndoTransaction* t, Change c) {
    if (t->len == t->cap) {
        int cap = t->cap ? t->cap * 2 : 4;
        auto* p = (Change*)realloc(t->changes, (size_t)cap * sizeof(Change));
        if (!p) {
            ChangeFree(&c);
            return;
        }
        t->changes = p;
        t->cap = cap;
    }
    t->changes[t->len++] = c;
}

static void StackClear(Vec<UndoTransaction>& v) {
    for (int i = 0; i < v.len; i++) {
        TransactionFree(&v[i]);
    }
    v.len = 0;
}

UndoManager::~UndoManager() {
    StackClear(undos);
    StackClear(redos);
    if (hasPending) {
        ChangeFree(&pending);
    }
}

// is_adjacent: whether the change coming in continues the one before it, which
// is what lets a run of the same intent stay one undo step.
static bool IsAdjacent(EditIntent intent, const Change& prev,
                       const Change& cur) {
    auto hasNewline = [](Str s) {
        for (int i = 0; i < s.len; i++) {
            if (s.s[i] == '\n' || s.s[i] == '\r') {
                return true;
            }
        }
        return false;
    };
    switch (intent) {
        case EditIntent::Typing:
            return prev.oldRange.IsEmpty() && cur.oldRange.IsEmpty() &&
                   !hasNewline(prev.newText) && !hasNewline(cur.newText) &&
                   prev.newRange.end == cur.oldRange.start;
        case EditIntent::Backspace:
            return prev.newText.len == 0 && cur.newText.len == 0 &&
                   cur.oldRange.end == prev.oldRange.start;
        case EditIntent::DeleteForward:
            return prev.newText.len == 0 && cur.newText.len == 0 &&
                   cur.oldRange.start == prev.oldRange.start;
        case EditIntent::Atomic:
            return false;
    }
    return false;
}

static bool RangeSame(Selection a, Selection b) {
    return a.start == b.start && a.end == b.end;
}

static void PushTransaction(UndoManager* m, Change change, EditIntent intent) {
    StackClear(m->redos);
    bool canCoalesce = false;
    if (!m->coalescingBoundary && intent != EditIntent::Atomic &&
        m->undos.len > 0) {
        UndoTransaction& prev = m->undos[m->undos.len - 1];
        canCoalesce = prev.intent == intent &&
                      prev.len < kMaxChangesPerTransaction && prev.len > 0 &&
                      IsAdjacent(intent, prev.changes[prev.len - 1], change);
    }
    if (canCoalesce) {
        TransactionPush(&m->undos[m->undos.len - 1], change);
        return;
    }
    if (m->undos.len >= kMaxUndoTransactions) {
        TransactionFree(&m->undos[0]);
        memmove(m->undos.els, m->undos.els + 1,
                (size_t)(m->undos.len - 1) * sizeof(UndoTransaction));
        m->undos.len--;
    }
    UndoTransaction t = {};
    t.intent = intent;
    TransactionPush(&t, change);
    m->undos.Append(t);
    m->coalescingBoundary = intent == EditIntent::Atomic;
}

void UndoRecordTransaction(UndoManager* m, Change change, EditIntent intent) {
    if (m->ignoring) {
        ChangeFree(&change);
        return;
    }
    // A no-op edit records nothing, but still ends the run before it, so the
    // undo history keeps whatever it already had.
    if (RangeSame(change.oldRange, change.newRange) &&
        StrSame(change.oldText, change.newText)) {
        ChangeFree(&change);
        UndoBreakCoalescing(m);
        return;
    }
    if (m->transactionOpen) {
        if (m->hasPending) {
            // The bracket keeps the first change's old side and takes the
            // latest new side, so the whole composition undoes at once.
            StrFree(m->pending.newText);
            m->pending.newRange = change.newRange;
            m->pending.newText = change.newText;
            m->pending.selAfter = change.selAfter;
            StrFree(change.oldText);
        } else {
            m->pending = change;
            m->hasPending = true;
        }
        return;
    }
    PushTransaction(m, change, intent);
}

void UndoBeginTransaction(UndoManager* m) {
    if (m->transactionOpen) {
        return;
    }
    m->transactionOpen = true;
    if (m->hasPending) {
        ChangeFree(&m->pending);
        m->hasPending = false;
    }
}

void UndoCommitTransaction(UndoManager* m) {
    if (!m->transactionOpen) {
        return;
    }
    m->transactionOpen = false;
    if (!m->hasPending) {
        return;
    }
    Change c = m->pending;
    m->hasPending = false;
    m->pending = {};
    if (!RangeSame(c.oldRange, c.newRange) || !StrSame(c.oldText, c.newText)) {
        PushTransaction(m, c, EditIntent::Atomic);
    } else {
        ChangeFree(&c);
    }
}

void UndoBreakCoalescing(UndoManager* m) {
    UndoCommitTransaction(m);
    m->coalescingBoundary = true;
}

bool UndoIsIgnoring(const UndoManager* m) {
    return m->ignoring;
}

void UndoSetIgnoring(UndoManager* m, bool ignoring) {
    m->ignoring = ignoring;
    if (ignoring) {
        UndoCommitTransaction(m);
    }
}

void UndoClear(UndoManager* m) {
    StackClear(m->undos);
    StackClear(m->redos);
    m->transactionOpen = false;
    if (m->hasPending) {
        ChangeFree(&m->pending);
        m->hasPending = false;
    }
    m->hasPendingIntent = false;
    m->coalescingBoundary = false;
}

const UndoTransaction* UndoPopUndo(UndoManager* m) {
    UndoCommitTransaction(m);
    if (m->undos.len == 0) {
        return nullptr;
    }
    UndoTransaction t = m->undos[m->undos.len - 1];
    m->undos.len--;
    m->redos.Append(t);
    m->coalescingBoundary = true;
    // The caller applies the changes in reverse, which is what Rust's
    // `.iter().rev()` hands it.
    return &m->redos[m->redos.len - 1];
}

const UndoTransaction* UndoPopRedo(UndoManager* m) {
    UndoCommitTransaction(m);
    if (m->redos.len == 0) {
        return nullptr;
    }
    UndoTransaction t = m->redos[m->redos.len - 1];
    m->redos.len--;
    m->undos.Append(t);
    m->coalescingBoundary = true;
    return &m->undos[m->undos.len - 1];
}

} // namespace gpui
