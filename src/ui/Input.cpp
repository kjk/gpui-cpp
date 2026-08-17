#include "ui/Input.h"
#include "ui/Primitive.h"

namespace gpui {

El* InputBase::New(Ctx* cx, Str id, int clickId) {
    Arena* a = cx->a;
    return UiRoot(a, id, clickId);
}

El* Input::New(Ctx* cx, LineInput* state) {
    Arena* a = cx->a;
    if (!state) {
        return TextEl(a, Str{});
    }
    Rgba fg = state->len > 0 ? Rgb(0x17, 0x17, 0x17) : Rgb(0x73, 0x73, 0x73);
    bool caret = state->focused && ((GetTickCount() / 500) % 2 == 0);
    El* bar = Div(a)->W(2)->H(14)->Bg(Rgb(0x17, 0x17, 0x17));
    El* slot = caret ? bar : Div(a)->W(2)->H(14);
    El* row = Div(a)->FlexRow()->ItemsCenter()->H(16);
    if (state->len <= 0) {
        // Overlay the caret so blinking does not shove the cue text.
        if (caret) {
            row->Child(bar->Absolute()->Left(0)->Top(1));
        }
        return row->Child(TextEl(a, Str(state->placeholder))->Font(12)->Fg(fg));
    }
    int cur = state->cursor;
    if (cur < 0) {
        cur = 0;
    }
    if (cur > state->len) {
        cur = state->len;
    }
    if (cur > 0) {
        row->Child(TextEl(a, Str(state->buf, cur))->Font(12)->Fg(fg));
    }
    if (state->focused) {
        row->Child(slot);
    }
    if (cur < state->len) {
        row->Child(TextEl(a, Str(state->buf + cur, state->len - cur))
                       ->Font(12)
                       ->Fg(fg));
    }
    return row;
}

El* Textarea::New(Ctx* cx, const char* text, bool caret) {
    Arena* a = cx->a;
    El* col = Div(a)->FlexCol();
    if (!text) {
        text = "";
    }
    int i = 0;
    for (;;) {
        int start = i;
        while (text[i] && text[i] != '\n') {
            i++;
        }
        char tmp[256];
        int n = i - start;
        if (n > 255) {
            n = 255;
        }
        memcpy(tmp, text + start, (size_t)n);
        tmp[n] = 0;
        bool last = text[i] == 0;
        El* line =
            TextEl(a, StrDup(a, Str(tmp)))->Font(12)->Fg(Rgb(0x17, 0x17, 0x17));
        if (last && caret) {
            El* row = Div(a)->FlexRow()->ItemsCenter()->H(16);
            row->Child(line);
            row->Child(Div(a)->W(2)->H(14)->Bg(Rgb(0x17, 0x17, 0x17)));
            col->Child(row);
        } else {
            col->Child(line);
        }
        if (last) {
            break;
        }
        i++;
    }
    return col;
}

static bool IsIdentChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

static bool TokEq(const char* s, int n, const char* kw) {
    int k = (int)strlen(kw);
    return n == k && memcmp(s, kw, (size_t)n) == 0;
}

static Rgba EditorTokColor(const char* s, int n) {
    static const char* kws[] = {"use",  "struct", "fn",    "impl",  "let",
                                "mut",  "pub",    "self",  "Self",  "as",
                                "in",   "for",    "if",    "else",  "return",
                                "true", "false",  "crate", "super", nullptr};
    for (int i = 0; kws[i]; i++) {
        if (TokEq(s, n, kws[i])) {
            return Rgb(0x25, 0x63, 0xeb);
        }
    }
    static const char* tys[] = {"usize", "isize",   "i32",  "u32",
                                "u64",   "i64",     "str",  "String",
                                "bool",  "HashMap", nullptr};
    for (int i = 0; tys[i]; i++) {
        if (TokEq(s, n, tys[i])) {
            return Rgb(0x25, 0x63, 0xeb);
        }
    }
    return Rgb(0x17, 0x17, 0x17);
}

static void EditorSpan(Arena* a, El* row, const char* s, int n, Rgba c) {
    if (n <= 0) {
        return;
    }
    row->Child(TextEl(a, StrDup(a, Str(s, n)))->Font(12)->Fg(c));
}

static void EditorCaret(Arena* a, El* row, bool on) {
    El* slot = Div(a)->W(2)->H(14);
    if (on) {
        slot->Bg(Rgb(0x17, 0x17, 0x17));
    }
    row->Child(slot);
}

static void EditorRun(Arena* a, El* row, const char* s, int n, Rgba c, int* col,
                      int caretCol, bool caretOn) {
    if (n <= 0) {
        return;
    }
    int start = *col;
    int end = start + n;
    if (caretCol < start || caretCol > end) {
        EditorSpan(a, row, s, n, c);
        *col = end;
        return;
    }
    int left = caretCol - start;
    if (left > 0) {
        EditorSpan(a, row, s, left, c);
    }
    EditorCaret(a, row, caretOn);
    if (n - left > 0) {
        EditorSpan(a, row, s + left, n - left, c);
    }
    *col = end;
}

static void HighlightEditorLine(Arena* a, El* row, const char* line, int n,
                                int caretCol, bool caretOn) {
    int i = 0;
    int col = 0;
    while (i < n && line[i] == ' ') {
        if (caretCol == col) {
            EditorCaret(a, row, caretOn);
        }
        EditorSpan(a, row, "\xC2\xB7", 2, Rgb(0xa3, 0xa3, 0xa3));
        col++;
        i++;
    }
    while (i < n) {
        if (line[i] == '/' && i + 1 < n && line[i + 1] == '/') {
            EditorRun(a, row, line + i, n - i, Rgb(0x73, 0x73, 0x73), &col,
                      caretCol, caretOn);
            i = n;
            break;
        }
        if (line[i] == '#') {
            int j = i + 1;
            while (j < n && line[j] != ']') {
                j++;
            }
            if (j < n) {
                j++;
            }
            EditorRun(a, row, line + i, j - i, Rgb(0x7c, 0x3a, 0xed), &col,
                      caretCol, caretOn);
            i = j;
            continue;
        }
        if (line[i] == '"') {
            int j = i + 1;
            while (j < n && line[j] != '"') {
                j++;
            }
            if (j < n) {
                j++;
            }
            EditorRun(a, row, line + i, j - i, Rgb(0x16, 0xa3, 0x4a), &col,
                      caretCol, caretOn);
            i = j;
            continue;
        }
        if (IsIdentChar(line[i]) && (line[i] < '0' || line[i] > '9')) {
            int j = i + 1;
            while (j < n && IsIdentChar(line[j])) {
                j++;
            }
            EditorRun(a, row, line + i, j - i, EditorTokColor(line + i, j - i),
                      &col, caretCol, caretOn);
            i = j;
            continue;
        }
        EditorRun(a, row, line + i, 1, Rgb(0x17, 0x17, 0x17), &col, caretCol,
                  caretOn);
        i++;
    }
    if (caretCol == col) {
        EditorCaret(a, row, caretOn);
    }
}

El* Editor::New(Ctx* cx, const char* text) {
    Arena* a = cx->a;
    return New(cx, text, -1, false);
}

El* Editor::New(Ctx* cx, const char* text, int cursor, bool caret) {
    Arena* a = cx->a;
    El* col = Div(a)->FlexCol();
    if (!text) {
        text = "";
    }
    bool blink = caret && ((GetTickCount() / 500) % 2 == 0);
    int i = 0;
    int pos = 0;
    int lineNo = 1;
    for (;;) {
        int start = i;
        while (text[i] && text[i] != '\n') {
            i++;
        }
        int n = i - start;
        int caretCol = -1;
        if (cursor >= pos && cursor <= pos + n) {
            caretCol = cursor - pos;
        }
        El* row = Div(a)->FlexRow()->H(20)->Gap(8)->ItemsCenter();
        row->Child(Div(a)->W(20)->JustifyEnd()->Child(
            TextEl(a, StrDup(a, fmt("%d", lineNo)))
                ->Font(11)
                ->Fg(Rgb(0xa3, 0xa3, 0xa3))));
        El* code = Div(a)->FlexRow()->ItemsCenter();
        HighlightEditorLine(a, code, text + start, n, caretCol, blink);
        row->Child(code);
        col->Child(row);
        lineNo++;
        if (!text[i]) {
            break;
        }
        i++;
        pos = i;
        if (lineNo > 80) {
            break;
        }
    }
    return col;
}
} // namespace gpui
