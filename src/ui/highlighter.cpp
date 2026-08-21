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
Highlighter* Highlighter::Language(Str name) {
    lang = SyntaxLangFor(name);
    return this;
}
Highlighter* Highlighter::Decorations(const TextSpan* runs, int n) {
    decorations = runs;
    nDecorations = n;
    return this;
}
Highlighter* Highlighter::ActiveLine(bool v) {
    activeLine = v;
    return this;
}
Highlighter* Highlighter::IndentGuides(bool v) {
    indentGuides = v;
    return this;
}

// The language's captures over the whole document, as runs the rows slice
// out of. Rust runs tree-sitter's highlights.scm and looks the capture names
// up in the HighlightTheme; the scanner in syntax.cpp answers a subset of the
// same names off the same table.
static int HighlightSpans(Ctx* cx, Str text, SyntaxLang lang, TextSpan* out,
                          int cap) {
    if (lang == SyntaxLangNone || !text.s) {
        return 0;
    }
    const Theme& th = cx->theme();
    ThemeMode mode = cx->themeMode();
    SyntaxLexer lx;
    SyntaxLexStart(&lx, lang, text);
    int n = 0;
    while (SyntaxLexNext(&lx) && n < cap) {
        if (lx.tok == SyntaxTok::Text) {
            continue;
        }
        Rgba c = SyntaxTokColor(lx.tok, mode, th.foreground);
        int lo = (int)(lx.text.s - text.s);
        int hi = lo + lx.text.len;
        // Adjacent runs of one colour are one span, which keeps a document
        // of a few hundred lines to a few hundred of them.
        if (n > 0 && out[n - 1].hi == lo && RgbaEq(out[n - 1].color, c)) {
            out[n - 1].hi = hi;
            continue;
        }
        out[n].lo = lo;
        out[n].hi = hi;
        out[n].color = c;
        out[n].bg = Rgba8(0, 0, 0, 0);
        out[n].underline = false;
        n++;
    }
    return n;
}

// The decorations win over the captures they overlap: every capture is cut
// back to what the decorations leave it, and the decorations go in whole.
// Both lists are in order, and so is the result.
static int MergeDecorations(TextSpan* spans, int n, const TextSpan* decs,
                            int nDecs, int cap) {
    auto* out = spans + 0;
    // Build into a scratch list, then copy back — the two runs interleave.
    TextSpan tmp[512];
    int m = 0;
    int i = 0;
    for (int d = 0; d < nDecs && m < 512; d++) {
        const TextSpan& dec = decs[d];
        for (; i < n && m < 512; i++) {
            TextSpan sp = spans[i];
            if (sp.hi <= dec.lo) {
                tmp[m++] = sp;
                continue;
            }
            if (sp.lo >= dec.hi) {
                break;
            }
            // The part before the decoration survives; the part after it is
            // put back for the next decoration to look at.
            if (sp.lo < dec.lo && m < 512) {
                TextSpan head = sp;
                head.hi = dec.lo;
                tmp[m++] = head;
            }
            if (sp.hi > dec.hi) {
                spans[i].lo = dec.hi;
                break;
            }
        }
        if (m < 512) {
            tmp[m++] = dec;
        }
    }
    for (; i < n && m < 512; i++) {
        tmp[m++] = spans[i];
    }
    if (m > cap) {
        m = cap;
    }
    for (int k = 0; k < m; k++) {
        out[k] = tmp[k];
    }
    return m;
}

Highlighter* Highlighter::Searchable(bool v) {
    searchable = v;
    return this;
}

El* Highlighter::IntoEl() {
    const Theme& th = cx->theme();
    if (state) {
        state->searchable = searchable;
    }
    InputEditorStyle style;
    style.foreground = th.foreground;
    style.mutedForeground = th.mutedFg;
    style.caret = th.caret;
    style.selection = RgbaOpacity(th.selection, 0.4f);
    style.fontSize = 12;
    // .font_family(theme.mono_font_family).text_size(theme.mono_font_size)
    style.mono = true;
    if (activeLine) {
        style.activeLine = RgbaOpacity(th.accent, 0.4f);
    }
    if (indentGuides) {
        style.indentGuide = RgbaOpacity(th.border, 0.8f);
    }
    // The language's captures first, the caller's decorations laid over them:
    // a decoration takes the range it covers away from whatever the scanner
    // said about it, which is what a TextDecorationCollection does.
    Str text = state ? InputValue(state) : Str{};
    const int kMaxSpans = 4096;
    auto* spans = (TextSpan*)Alloc(a, (int)sizeof(TextSpan) * kMaxSpans);
    int n = HighlightSpans(cx, text, lang, spans, kMaxSpans);
    if (nDecorations > 0) {
        n = MergeDecorations(spans, n, decorations, nDecorations, kMaxSpans);
    }
    style.spans = n > 0 ? spans : nullptr;
    style.nSpans = n;
    // layout_search_matches: the matches are painted only while the bar is
    // open, which is when Rust builds paths for them at all.
    if (state && state->search.open) {
        const SearchMatcher* m = &state->search.matcher;
        style.matches = m->ranges.els;
        style.nMatches = m->ranges.len;
        style.currentMatch = SearchMatcherIndex(m);
        style.matchBg = RgbaOpacity(th.warning, 0.35f);
        style.currentMatchBg = RgbaOpacity(th.warning, 0.75f);
    }
    El* editor = gpui::Editor::New(cx, state, style);
    El* scroller = editor;
    if (h > 0) {
        // The scroll handle is the editor's: the rows slide under this box as
        // the caret moves, and the wheel moves them too.
        scroller = InputBase::New(cx, id, HashClickId(id))
                       ->BindInput(state)
                       ->FlexCol()
                       ->W(kFill)
                       ->H(h)
                       ->ClipY()
                       ->ScrollY(state ? state->scrollY : 0)
                       ->Child(editor);
    }
    if (!searchable) {
        return scroller;
    }
    // The bar sits over the rows and takes its height off them, the way
    // Rust's overlay docks it at the top of the input.
    return Div(a)
        ->FlexCol()
        ->W(kFill)
        ->Child(SearchPanel::New(cx, StrDup(a, fmt("%s-search", id)), state)
                    ->IntoEl())
        ->Child(scroller);
}

} // namespace component
} // namespace gpui
