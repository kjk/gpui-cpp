#include "component/Text.h"

namespace gpui {

namespace component {

// ─── inline ───────────────────────────────────────────────────────────────
//
// crates/ui/src/text/inline.rs builds a real inline flow over an mdast
// paragraph; this is the part of it the examples reach for.

static bool HasEmphasis(Str s) {
    for (int i = 0; i < s.len; i++) {
        if (s.s[i] == '*') {
            return true;
        }
    }
    return false;
}

// A run with no emphasis stays one TextEl, so the text engine keeps breaking
// the line on its own metrics and the common case costs nothing. Only a run
// that changes weight part-way is split into a wrapping row of styled words.
//
// Each word carries its own trailing space rather than the row carrying a
// gap: a gap puts a space between an emphasis run and the punctuation after
// it ("**bold**:" reads as "bold :") and loosens wrapped-line leading.
static El* Inline(Arena* a, Str s, float font, Rgba color, bool bold,
                  bool selectable) {
    if (!HasEmphasis(s)) {
        El* t = TextEl(a, s)->Font(font)->Fg(color)->Wrap();
        if (bold) {
            t->Semibold();
        }
        if (selectable) {
            t->Selectable();
        }
        return t;
    }
    El* row = Div(a)->FlexRow()->FlexWrap();
    bool strong = false;
    bool italic = false;
    char word[256];
    int n = 0;
    auto flush = [&]() {
        if (n <= 0) {
            return;
        }
        El* t = TextEl(a, StrDup(a, Str(word, n)))->Font(font)->Fg(color);
        if (strong || bold) {
            t->Semibold();
        }
        if (italic) {
            t->Italic();
        }
        if (selectable) {
            t->Selectable();
        }
        row->Child(t);
        n = 0;
    };
    for (int i = 0; i <= s.len; i++) {
        char c = i < s.len ? s.s[i] : 0;
        if (c == '*') {
            flush();
            // ** toggles weight, a lone * toggles slant.
            if (i + 1 < s.len && s.s[i + 1] == '*') {
                strong = !strong;
                i++;
            } else {
                italic = !italic;
            }
            continue;
        }
        if (c == ' ' || c == 0) {
            if (c == ' ' && n < (int)sizeof(word) - 1) {
                word[n++] = ' ';
            }
            flush();
            continue;
        }
        if (n < (int)sizeof(word) - 1) {
            word[n++] = c;
        }
    }
    return row;
}

// ─── blocks ───────────────────────────────────────────────────────────────

// node.rs 2258. h1 is BOLD, h2 through h6 SEMIBOLD.
static float HeadingScale(int level) {
    switch (level) {
        case 1:
            return 2.f;
        case 2:
            return 1.5f;
        case 3:
            return 1.25f;
        case 4:
            return 1.125f;
        default:
            return 1.f;
    }
}

// Leading '#' run, 1..6, or 0 for anything else.
static int HeadingLevel(const char* line, int llen, int* textStart) {
    int i = 0;
    while (i < llen && i < 6 && line[i] == '#') {
        i++;
    }
    if (i == 0 || i >= llen || line[i] != ' ') {
        return 0;
    }
    while (i < llen && line[i] == ' ') {
        i++;
    }
    *textStart = i;
    return i;
}

static bool IsRule(const char* line, int llen) {
    if (llen < 3) {
        return false;
    }
    for (int i = 0; i < llen; i++) {
        if (line[i] != '-' && line[i] != ' ') {
            return false;
        }
    }
    return true;
}

static bool IsTableLine(const char* s, int llen) {
    return llen > 0 && s[0] == '|';
}

static bool IsBullet(const char* line, int llen, int* textStart) {
    if (llen >= 2 && (line[0] == '-' || line[0] == '*') && line[1] == ' ') {
        *textStart = 2;
        return true;
    }
    return false;
}

static bool IsOrdered(const char* line, int llen, int* textStart) {
    int i = 0;
    while (i < llen && line[i] >= '0' && line[i] <= '9') {
        i++;
    }
    if (i == 0 || i + 1 >= llen || line[i] != '.' || line[i + 1] != ' ') {
        return false;
    }
    *textStart = i + 2;
    return true;
}

El* TextView::IntoEl() {
    const Theme& th = cx->theme();
    El* col = Div(a)->FlexCol()->Gap(10);
    if (!source.s || source.len <= 0) {
        return col;
    }
    // The heading level's own run of '#' is consumed above; what is left is a
    // marker column for lists and the text itself.
    const char* p = source.s;
    const char* end = source.s + source.len;
    while (p < end) {
        const char* line = p;
        while (p < end && *p != '\n') {
            p++;
        }
        int llen = (int)(p - line);
        if (p < end && *p == '\n') {
            p++;
        }

        int at = 0;
        int level = HeadingLevel(line, llen, &at);
        if (level > 0) {
            float font = baseFont * HeadingScale(level);
            col->Child(Inline(a, Str((char*)line + at, llen - at), font,
                              th.foreground, true, selectable)
                           ->W(kFill));
            continue;
        }
        if (IsRule(line, llen)) {
            col->Child(Div(a)->H(1)->W(kFill)->Bg(th.border));
            continue;
        }
        if (IsBullet(line, llen, &at)) {
            // "•" is Rust's depth-0 bullet, crates/ui/src/text/utils.rs.
            col->Child(ListRow(StrL("\xE2\x80\xA2"),
                               Str((char*)line + at, llen - at)));
            continue;
        }
        if (IsOrdered(line, llen, &at)) {
            col->Child(ListRow(StrDup(a, Str((char*)line, at - 1)),
                               Str((char*)line + at, llen - at)));
            continue;
        }
        if (IsTableLine(line, llen)) {
            p = Table(col, line, end);
            continue;
        }
        if (llen > 0) {
            col->Child(Inline(a, Str((char*)line, llen), baseFont,
                              th.foreground, false, selectable)
                           ->W(kFill));
        }
    }
    return col;
}

// A list item: the marker in a fixed gutter, the text wrapping beside it.
El* TextView::ListRow(Str marker, Str text) {
    const Theme& th = cx->theme();
    return Div(a)
        ->FlexRow()
        ->W(kFill)
        ->Gap(8)
        ->Child(Div(a)->W(16)->Shrink0()->Child(
            TextEl(a, marker)->Font(baseFont)->Fg(th.mutedFg)))
        ->Child(Inline(a, text, baseFont, th.foreground, false, selectable)
                    ->Grow());
}

// Consumes the run of table lines starting at `line`; returns where the
// source continues.
const char* TextView::Table(El* col, const char* line, const char* end) {
    const Theme& th = cx->theme();
    El* table = Div(a)->FlexCol()->Border(1, th.border);
    int rows = 0;
    const char* q = line;
    while (q < end) {
        const char* e = q;
        while (e < end && *e != '\n') {
            e++;
        }
        if (!IsTableLine(q, (int)(e - q))) {
            break;
        }
        // A |---|:--:| separator carries alignment, which this does not use.
        bool sep = true;
        for (const char* c = q; c < e; c++) {
            if (*c != '|' && *c != '-' && *c != ':' && *c != ' ') {
                sep = false;
                break;
            }
        }
        if (!sep) {
            El* row = Div(a)->FlexRow();
            if (rows == 0) {
                row->Bg(th.muted);
            } else if (rows % 2 == 0) {
                row->Bg(th.tableEven);
            }
            const char* cell = q + 1;
            while (cell < e) {
                const char* bar = cell;
                while (bar < e && *bar != '|') {
                    bar++;
                }
                int cl = (int)(bar - cell);
                while (cl > 0 && cell[0] == ' ') {
                    cell++;
                    cl--;
                }
                while (cl > 0 && cell[cl - 1] == ' ') {
                    cl--;
                }
                row->Child(Div(a)->W(tableColW)->PadX(8)->PadY(6)->Child(
                    Inline(a, Str((char*)cell, cl), baseFont - 2, th.foreground,
                           false, selectable)
                        ->W(kFill)));
                if (bar >= e) {
                    break;
                }
                cell = bar + 1;
            }
            table->Child(row);
            rows++;
        }
        if (e < end && *e == '\n') {
            e++;
        }
        q = e;
    }
    col->Child(table);
    return q;
}

// ─── builder ──────────────────────────────────────────────────────────────

TextView* TextView::New(Ctx* cx, Str source) {
    Arena* a = cx->a;
    TextView* t = ArenaNew<TextView>(a);
    t->a = a;
    t->cx = cx;
    t->source = source;
    return t;
}

TextView* TextView::Font(float px) {
    baseFont = px;
    return this;
}

TextView* TextView::Selectable(bool on) {
    selectable = on;
    return this;
}

TextView* TextView::TableColumnWidth(float px) {
    tableColW = px;
    return this;
}

} // namespace component
} // namespace gpui
