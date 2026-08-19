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

#include "gpui/Gpui.h"

namespace gpui {

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
    Selection r = range ? *range : s->selectedRange;
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
    UpdatePreferredColumn(s);
    if (InputIsMultiLine(s) && s->mode.kind == LayoutModeKind::AutoGrow) {
        LayoutModeSetRows(&s->mode, RopeLinesLen(InputValue(s)));
    }
    Emit(s, app, win, InputEvent{InputEventKind::Change});
    Notify(app, win);
    return true;
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

} // namespace gpui
