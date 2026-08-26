#include "ui/label.h"

namespace gpui {

namespace component {

Label* Label::New(Ctx* cx, Str text) {
    Arena* a = cx->a;
    Label* l = ArenaNew<Label>(a);
    l->a = a;
    l->cx = cx;
    l->text = text;
    return l;
}

Label* Label::Secondary(Str s) {
    secondary = s;
    return this;
}

Label* Label::Masked(bool v) {
    masked = v;
    return this;
}
Label* Label::Semibold() {
    semibold = true;
    return this;
}
Label* Label::Font(float px) {
    font = px;
    return this;
}
Label* Label::Highlights(Str matched, bool prefix) {
    highlights = matched;
    prefixMatch = prefix;
    return this;
}
Label* Label::TextCenter() {
    align = 1;
    return this;
}
Label* Label::TextRight() {
    align = 2;
    return this;
}
Label* Label::LineHeight(float mult) {
    lineHeight = mult;
    return this;
}

// ASCII-insensitive compare of `n` bytes.
static bool LabelEqI(const char* a, const char* b, int n) {
    for (int i = 0; i < n; i++) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') {
            x = (char)(x - 'A' + 'a');
        }
        if (y >= 'A' && y <= 'Z') {
            y = (char)(y - 'A' + 'a');
        }
        if (x != y) {
            return false;
        }
    }
    return true;
}

// highlight_ranges: the run at the start for a prefix match, every occurrence
// for a full one. The needle is matched case-insensitively, as Rust lowers
// both sides before searching.
static El* LabelHighlighted(Arena* a, Ctx* cx, Str text, Str needle,
                            bool prefix, Rgba fg, float font, bool semibold) {
    const Theme& th = ThemeNow(cx->app);
    El* row = Div(a)->FlexRow()->ItemsCenter();
    auto piece = [&](int from, int to, bool hit) {
        if (to <= from) {
            return;
        }
        El* t = TextEl(a, Str(text.s + from, to - from))
                    ->Font(font)
                    ->Fg(hit ? th.blue : fg);
        if (semibold) {
            t->Semibold();
        }
        row->Child(t);
    };
    int at = 0;
    if (needle.len > 0 && needle.len <= text.len) {
        if (prefix) {
            if (LabelEqI(text.s, needle.s, needle.len)) {
                piece(0, needle.len, true);
                at = needle.len;
            }
        } else {
            int i = 0;
            while (i + needle.len <= text.len) {
                if (LabelEqI(text.s + i, needle.s, needle.len)) {
                    piece(at, i, false);
                    piece(i, i + needle.len, true);
                    at = i + needle.len;
                    i = at;
                } else {
                    i++;
                }
            }
        }
    }
    piece(at, text.len, false);
    return row;
}

El* Label::IntoEl() {
    const Theme& th = ThemeNow(cx->app);
    Str shown = text;
    if (masked && text.len > 0) {
        char buf[64];
        int n = text.len < 63 ? text.len : 63;
        // ASCII bullets
        for (int i = 0; i < n; i++) {
            buf[i] = '*';
        }
        buf[n] = 0;
        shown = StrDup(a, Str(buf, n));
    }
    El* row = Div(a)->FlexRow()->ItemsCenter()->Gap(6)->W(kFill);
    if (align == 1) {
        row->JustifyCenter();
    } else if (align == 2) {
        row->JustifyEnd();
    }
    if (lineHeight > 0) {
        row->LineHeight(lineHeight);
    }
    if (highlights.len > 0 && !masked) {
        row->Child(LabelHighlighted(a, cx, shown, highlights, prefixMatch,
                                    th.foreground, font, semibold));
    } else {
        El* primary = TextEl(a, shown)->Font(font)->Fg(th.foreground);
        if (semibold) {
            primary->Semibold();
        }
        row->Child(primary);
    }
    if (secondary.s) {
        // The secondary text is part of the run a highlight searches, so it
        // lights up too.
        if (highlights.len > 0 && !masked) {
            row->Child(LabelHighlighted(a, cx, secondary, highlights, false,
                                        th.mutedFg, 14, false));
        } else {
            row->Child(TextEl(a, secondary)->Font(14)->Fg(th.mutedFg));
        }
    }
    return row;
}

} // namespace component
} // namespace gpui
