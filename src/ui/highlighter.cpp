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
                            int nDecs, int cap, TextSpan* tmp) {
    auto* out = spans + 0;
    // Build into the caller's scratch, then copy back — the two runs
    // interleave, and the scratch is as long as the result may be, so a
    // document with more captures than a fixed one would hold does not lose
    // the tail of its colours.
    int m = 0;
    int i = 0;
    for (int d = 0; d < nDecs && m < cap; d++) {
        const TextSpan& dec = decs[d];
        for (; i < n && m < cap; i++) {
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
            if (sp.lo < dec.lo && m < cap) {
                TextSpan head = sp;
                head.hi = dec.lo;
                tmp[m++] = head;
            }
            if (sp.hi > dec.hi) {
                spans[i].lo = dec.hi;
                break;
            }
        }
        if (m < cap) {
            tmp[m++] = dec;
        }
    }
    for (; i < n && m < cap; i++) {
        tmp[m++] = spans[i];
    }
    for (int k = 0; k < m; k++) {
        out[k] = tmp[k];
    }
    return m;
}

Highlighter* Highlighter::Folding(bool v) {
    folding = v;
    return this;
}

Highlighter* Highlighter::Searchable(bool v) {
    searchable = v;
    return this;
}

/* Fold candidates — crates/ui/src/highlighter/input_adapter.rs
   extract_fold_ranges.

   Rust walks the tree-sitter tree and offers every named node that spans two
   rows or more; the showcase's own highlighter, which has no tree either,
   scans the text for brace pairs instead (`brace_fold_ranges` in
   examples/showcase/syntect_highlighter.rs). This is the second of the two,
   run over the lexer rather than over raw characters — so a brace inside a
   string or a comment is not a brace, which is what the hand-rolled quote and
   `//` tracking in Rust's version is doing by hand and does less well.

   A language with no braces has no candidates, which is where this stops
   short of the tree: Rust would fold a Python suite and this cannot see one.
*/

// Rust bounds this by the tree; a document with more foldable blocks than
// this has more chevrons than a gutter can show anyway.
static const int kMaxFoldRanges = 512;

static int FoldCandidates(Str text, SyntaxLang lang, FoldRange* out, int cap) {
    if (text.len == 0 || cap <= 0) {
        return 0;
    }
    // The line each byte is on, walked alongside the scan so a token's line
    // is known without searching for it.
    int starts[64];
    int nOpen = 0;
    int n = 0;
    int line = 0;
    int at = 0;
    SyntaxLexer lx = {};
    SyntaxLexStart(&lx, lang, text);
    while (SyntaxLexNext(&lx)) {
        int tokStart = (int)(lx.text.s - text.s);
        // Everything between the last token and this one, plus the token
        // itself when it is not one that can hold a brace.
        for (; at < tokStart && at < text.len; at++) {
            if (text.s[at] == '\n') {
                line++;
            }
        }
        bool literal =
            lx.tok == SyntaxTok::String || lx.tok == SyntaxTok::Comment;
        for (int i = 0; i < lx.text.len; i++) {
            char c = lx.text.s[i];
            if (c == '\n') {
                line++;
                continue;
            }
            if (literal) {
                continue;
            }
            if (c == '{') {
                if (nOpen < (int)(sizeof(starts) / sizeof(starts[0]))) {
                    starts[nOpen] = line;
                }
                nOpen++;
            } else if (c == '}' && nOpen > 0) {
                nOpen--;
                if (nOpen >= (int)(sizeof(starts) / sizeof(starts[0]))) {
                    continue;
                }
                int startLine = starts[nOpen];
                // A block that opens and closes on one line has nothing to
                // hide, and Rust drops it the same way.
                if (startLine < line && n < cap) {
                    out[n].startLine = startLine;
                    out[n].endLine = line;
                    n++;
                }
            }
        }
        at = tokStart + lx.text.len;
    }
    return n;
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
    // theme.mono_font_size, which is 13 rather than the 12 this drew at: a
    // narrower row is a row that does not soft-wrap where Rust's does.
    style.fontSize = 13;
    // .font_family(theme.mono_font_family).text_size(theme.mono_font_size)
    style.mono = true;
    if (activeLine) {
        style.activeLine = RgbaOpacity(th.accent, 0.4f);
    }
    if (indentGuides) {
        style.indentGuide = RgbaOpacity(th.border, 0.8f);
    }
    if (state) {
        // This façade is Rust's code editor — the highlighter is bound to
        // an EditorState, whose LayoutMode is CodeEditor — so it is what
        // says so, rather than every caller having to.
        state->mode.kind = LayoutModeKind::CodeEditor;
        state->mode.folding = folding;
    }
    if (state && folding) {
        FoldRange ranges[kMaxFoldRanges];
        int nRanges =
            FoldCandidates(InputValue(state), lang, ranges, kMaxFoldRanges);
        InputSetFoldCandidates(state, ranges, nRanges);
    }
    // The language's captures first, the caller's decorations laid over them:
    // a decoration takes the range it covers away from whatever the scanner
    // said about it, which is what a TextDecorationCollection does.
    Str text = state ? InputValue(state) : Str{};
    const int kMaxSpans = 4096;
    auto* spans = (TextSpan*)Alloc(a, (int)sizeof(TextSpan) * kMaxSpans);
    int n = HighlightSpans(cx, text, lang, spans, kMaxSpans);
    if (nDecorations > 0) {
        auto* tmp = (TextSpan*)Alloc(a, (int)sizeof(TextSpan) * kMaxSpans);
        if (tmp) {
            n = MergeDecorations(spans, n, decorations, nDecorations, kMaxSpans,
                                 tmp);
        }
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
