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

Highlighter* Highlighter::Diagnostics(const Diagnostic* items, int n) {
    diagnostics = items;
    nDiagnostics = n;
    return this;
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
    // EditorStyle::diagnostics — theme.danger, warning, info and the muted
    // foreground a hint is drawn in.
    // highlighter/registry.rs's own defaults for the status colours a
    // diagnostic is drawn in.
    style.diagnostics.error = th.red;
    style.diagnostics.warning = th.yellow;
    style.diagnostics.info = th.blue;
    style.diagnostics.hint = th.cyan;
    if (state) {
        // The set the caller published, kept on the state so the row builder
        // and a hover both read the same one.
        state->diagnostics.Clear();
        for (int i = 0; i < nDiagnostics; i++) {
            state->diagnostics.Append(diagnostics[i]);
        }
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
    // The rows are virtualized against the box they scroll in, and paint only
    // learns its height a frame later; the builder knows it now.
    if (state && h > 0) {
        state->viewH = h;
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
    // The completion menu, under the caret: the list on the left and the
    // selected item's documentation beside it, which is what CompletionMenu
    // defers into place.
    El* completionMenu = nullptr;
    if (state && state->completion.open && state->completion.items.len > 0) {
        // `cursor_origin + (-4, line_height + 4)`, in window coordinates —
        // the caret's own box is what the editor measured last frame.
        float x = state->caretWinX > 0 ? state->caretWinX - 4.f
                                       : state->inputBounds.x;
        // The caret's row. Wrapped rows report their boxes and the menu is
        // placed under the one the caret is on; without wrapping every row is
        // a line high and the arithmetic is the same one the caret's own
        // scrolling uses.
        float y = state->caretWinY > 0
                      ? state->caretWinY + 4.f
                      : state->inputBounds.y + kInputLineH + 4.f;
        El* list = Div(a)
                       ->FlexCol()
                       ->MinW(160)
                       ->MaxW(420)
                       ->MaxH(240)
                       ->ClipY()
                       ->Pad(4)
                       ->Radius(th.radius)
                       ->Bg(th.tokens.background)
                       ->Border(1, th.border);
        for (int i = 0; i < state->completion.items.len; i++) {
            const CompletionItem& item = state->completion.items[i];
            bool selected = i == state->completion.selected;
            El* row = Div(a)
                          ->FlexRow()
                          ->Gap(8)
                          ->Pad(4)
                          ->ItemsCenter()
                          ->Radius(th.radius * 0.5f)
                          ->Font(12);
            if (selected) {
                row->Bg(th.tokens.accent)->Fg(th.accentFg);
            }
            El* label = TextEl(a, item.label)->LineHeight(1.f);
            if (item.deprecated) {
                label->Strikethrough();
            }
            row->Child(label);
            if (item.detail.len > 0) {
                row->Child(TextEl(a, item.detail)
                               ->LineHeight(1.f)
                               ->Italic()
                               ->Fg(selected ? th.accentFg : th.mutedFg));
            }
            list->Child(row);
        }
        El* menu = Div(a)->FlexRow()->Gap(4)->ItemsStart()->Child(list);
        // The documentation of the item the selection is on, beside the list.
        int sel = state->completion.selected;
        if (sel >= 0 && sel < state->completion.items.len &&
            state->completion.items[sel].documentation.len > 0) {
            menu->Child(
                Div(a)
                    ->W(420)
                    ->MaxH(240)
                    ->ClipY()
                    ->PadX(8)
                    ->PadY(4)
                    ->Radius(th.radius)
                    ->Bg(th.tokens.background)
                    ->Border(1, th.border)
                    ->Child(TextView::New(cx, state->completion.items[sel]
                                                  .documentation)
                                ->Font(12)
                                ->IntoEl()));
        }
        completionMenu = Div(a)->Fixed()->Left(x)->Top(y)->Child(menu);
    }

    // The diagnostic popover: what the pointer is over, in the severity's
    // own colours — `px_1().py_0p5()` over a background the colour is blended
    // a fifth into, with the message as markdown.
    El* diagPopover = nullptr;
    if (state && state->hoverDiagnostic >= 0 &&
        state->hoverDiagnostic < state->diagnostics.len) {
        const Diagnostic& dg = state->diagnostics[state->hoverDiagnostic];
        Rgba fg = style.diagnostics.info;
        if (dg.severity == DiagnosticSeverity::Error) {
            fg = style.diagnostics.error;
        } else if (dg.severity == DiagnosticSeverity::Warning) {
            fg = style.diagnostics.warning;
        } else if (dg.severity == DiagnosticSeverity::Hint) {
            fg = style.diagnostics.hint;
        }
        // `bg.blend(colour.alpha(0.2))`: a fifth of the status colour over
        // the window's own background. RgbaMix weights its first argument, so
        // the background is the one that takes the four fifths.
        Rgba bg = RgbaMix(th.background, fg, 0.8f);
        El* body = TextView::New(cx, dg.message)->Font(12)->IntoEl();
        diagPopover = Div(a)
                          ->Fixed()
                          ->Left(state->hoverDiagnosticX + 8)
                          ->Top(state->hoverDiagnosticY + 18)
                          ->MaxW(420)
                          ->PadX(4)
                          ->PadY(2)
                          ->Radius(th.radius)
                          ->Bg(bg)
                          ->Fg(fg)
                          ->Border(1, fg)
                          ->Child(body);
    }
    // The hover popover: what the provider said about the word the pointer is
    // resting on, as markdown in a plain popover — HoverPopover, which is the
    // diagnostic's neighbour without the severity colouring.
    El* hoverPopover = nullptr;
    if (state && state->hoverText.len > 0 && state->hoverDiagnostic < 0) {
        hoverPopover = Div(a)
                           ->Fixed()
                           ->Left(state->hoverX + 8)
                           ->Top(state->hoverY + 18)
                           ->MinW(200)
                           ->MaxW(500)
                           ->PadX(8)
                           ->PadY(4)
                           ->Radius(th.radius)
                           ->Bg(th.tokens.background)
                           ->Border(1, th.border)
                           ->Child(TextView::New(cx, state->hoverText)
                                       ->Font(12)
                                       ->IntoEl());
    }
    if (hoverPopover && !diagPopover) {
        diagPopover = hoverPopover;
    }
    if (completionMenu) {
        // The menu is over the rows either way, so it goes in beside them
        // rather than inside the scroller.
        El* box =
            Div(a)->FlexCol()->W(kFill)->Child(scroller)->Child(completionMenu);
        if (diagPopover) {
            box->Child(diagPopover);
        }
        if (!searchable) {
            return box;
        }
        return Div(a)
            ->FlexCol()
            ->W(kFill)
            ->Child(SearchPanel::New(cx, StrDup(a, fmt("%s-search", id)), state)
                        ->IntoEl())
            ->Child(box);
    }
    if (!searchable) {
        if (diagPopover) {
            return Div(a)->FlexCol()->W(kFill)->Child(scroller)->Child(
                diagPopover);
        }
        return scroller;
    }
    // The bar sits over the rows and takes its height off them, the way
    // Rust's overlay docks it at the top of the input.
    El* box =
        Div(a)
            ->FlexCol()
            ->W(kFill)
            ->Child(SearchPanel::New(cx, StrDup(a, fmt("%s-search", id)), state)
                        ->IntoEl())
            ->Child(scroller);
    if (diagPopover) {
        box->Child(diagPopover);
    }
    return box;
}

} // namespace component
} // namespace gpui
