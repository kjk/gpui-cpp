#include "ui/highlighter.h"
#include "ui/input.h"
#include "ui/popover.h"
#include "ui/text.h"
#include "base/positioner.h"

namespace gpui {

namespace component {

// CompletionMenu and CodeActionMenu are retained entities in Rust. Their
// durable data already lives on InputState here; this small keyed entity is
// the listener owner that gives the frame rows the same pointer behavior.
struct InputMenuViewState {
    InputState* input = nullptr;

    static void CompletionClick(InputMenuViewState* self, Ctx* cx,
                                const ClickEvent*, intptr_t ix) {
        if (!self || !self->input || ix < 0 ||
            ix >= self->input->completion.items.len) {
            return;
        }
        self->input->completion.selected = (int)ix;
        InputAcceptCompletion(self->input, cx->app, cx->win);
        Notify(cx);
    }

    static void CompletionHover(InputMenuViewState* self, Ctx* cx,
                                const HoverEvent* event, intptr_t ix) {
        if (!self || !self->input || !event || !event->hovered || ix < 0 ||
            ix >= self->input->completion.items.len ||
            self->input->completion.selected == ix) {
            return;
        }
        self->input->completion.selected = (int)ix;
        Notify(cx);
    }

    static void CompletionOutside(InputMenuViewState* self, Ctx* cx,
                                  const MouseDownEvent*) {
        if (!self || !self->input) return;
        InputDismissCompletion(self->input);
        Notify(cx);
    }

    static void CodeActionClick(InputMenuViewState* self, Ctx* cx,
                                const ClickEvent*, intptr_t ix) {
        if (!self || !self->input || ix < 0 ||
            ix >= self->input->codeActions.items.len) {
            return;
        }
        self->input->codeActions.selected = (int)ix;
        InputPerformCodeAction(self->input, cx->app, cx->win);
        Notify(cx);
    }

    static void CodeActionHover(InputMenuViewState* self, Ctx* cx,
                                const HoverEvent* event, intptr_t ix) {
        if (!self || !self->input || !event || !event->hovered || ix < 0 ||
            ix >= self->input->codeActions.items.len ||
            self->input->codeActions.selected == ix) {
            return;
        }
        self->input->codeActions.selected = (int)ix;
        Notify(cx);
    }

    static void CodeActionOutside(InputMenuViewState* self, Ctx* cx,
                                  const MouseDownEvent*) {
        if (!self || !self->input) return;
        InputDismissCodeActions(self->input);
        Notify(cx);
    }

    static void PopoverOutside(InputMenuViewState* self, Ctx* cx,
                               const MouseDownEvent*) {
        if (!self || !self->input) return;
        self->input->hoverText = {};
        self->input->hoverRange = {};
        self->input->hoverAsked = true;
        self->input->hoverDiagnostic = -1;
        self->input->popoverBounds = {};
        Notify(cx);
    }
};

static Entity<InputMenuViewState> InputMenuView(Ctx* cx, InputState* input,
                                                 const char* kind) {
    Entity<InputMenuViewState> entity = ElementStateEntity<InputMenuViewState>(
        cx, fmt("%s-%p", Str(kind), (void*)input),
        StrL("gpui::component::InputMenuViewState"));
    if (InputMenuViewState* state = entity.Get(cx)) state->input = input;
    return entity;
}

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
Highlighter* Highlighter::Font(float px) {
    fontSize = px;
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

CompletionMenu* CompletionMenu::New(Ctx* cx, InputState* editor) {
    CompletionMenu* menu = ArenaNew<CompletionMenu>(cx->a);
    menu->a = cx->a;
    menu->cx = cx;
    menu->editor = editor;
    return menu;
}

CompletionMenu* CompletionMenu::UpdateQuery(int startOffset, Str value) {
    query = value;
    if (editor && editor->completion.triggerStart < 0) {
        editor->completion.triggerStart = startOffset;
    }
    return this;
}

CompletionMenu* CompletionMenu::Show(int offset,
                                     const CompletionItem* items, int n) {
    if (editor) {
        int start = editor->completion.triggerStart >= 0
                        ? editor->completion.triggerStart
                        : offset;
        InputPresentCompletionItems(editor, start, query, items, n);
        editor->completion.offset = offset;
        if (cx->win) AppInvalidate(cx->win);
    }
    return this;
}

void CompletionMenu::Hide() {
    InputDismissCompletion(editor);
    if (cx && cx->win) AppInvalidate(cx->win);
}

bool CompletionMenu::HandleAction(InputAction action) {
    return editor &&
           InputCompletionAction(editor, cx->app, cx->win, action);
}

El* CompletionMenu::IntoEl() {
    if (!editor || !editor->completion.open ||
        editor->completion.items.len <= 0) {
        return nullptr;
    }
    const Theme& theme = ThemeNow(cx->app);
    float x = editor->caretWinX > 0 ? editor->caretWinX - 4.f
                                    : editor->inputBounds.x;
    float lineH = editor->lastLineH > 0 ? editor->lastLineH : 20.f;
    float y = editor->caretWinY > 0 ? editor->caretWinY + 4.f
                                    : editor->inputBounds.y + lineH + 4.f;
    float configuredMax = editor->completionMenuMaxW;
    float windowW = cx->win ? WindowSize(cx->win).dipW : 0.f;
    float maxW = configuredMax;
    if (windowW > 0 && windowW - x < maxW) maxW = windowW - x;
    if (maxW < 120.f) maxW = 120.f;
    const float gap = 4.f;
    bool vertical = windowW > 0 &&
                    x + configuredMax + gap + configuredMax + gap > windowW;
    Entity<InputMenuViewState> view =
        InputMenuView(cx, editor, "completion-menu");

    El* list = PopoverSurface(
        cx, Div(a)->FlexCol()->MinW(120)->MaxW(maxW)->MaxH(240)->ClipY()->Pad(
                4));
    for (int i = 0; i < editor->completion.items.len; i++) {
        const CompletionItem& item = editor->completion.items[i];
        bool selected = i == editor->completion.selected;
        El* row = Div(a)
                      ->FlexRow()
                      ->Gap(8)
                      ->Pad(4)
                      ->ItemsCenter()
                      ->Radius(theme.radius * 0.5f)
                      ->Font(12)
                      ->HoverBg(BackgroundOpacity(theme.tokens.accent, 0.8f))
                      ->OnClick(ListenTo(view, &InputMenuViewState::CompletionClick,
                                         i))
                      ->OnHover(ListenTo(view, &InputMenuViewState::CompletionHover,
                                         i));
        if (selected) row->Bg(theme.tokens.accent)->Fg(theme.accentFg);
        El* label = TextEl(a, item.label)->LineHeight(1.f);
        int matched = editor->completion.query.len;
        if (matched > item.label.len) matched = item.label.len;
        if (matched > 0) {
            TextSpan* prefix = ArenaNew<TextSpan>(a);
            prefix->lo = 0;
            prefix->hi = matched;
            prefix->color = theme.blue;
            label->Spans(prefix, 1);
        }
        if (item.deprecated) label->Strikethrough();
        row->Child(label);
        if (item.detail.len > 0) {
            El* detail = TextEl(a, item.detail)
                              ->LineHeight(1.f)
                              ->Italic()
                              ->Fg(selected ? theme.accentFg : theme.mutedFg);
            if (item.deprecated) detail->Strikethrough();
            row->Child(detail);
        }
        list->Child(row);
    }
    El* menu = Div(a)->Gap(gap)->ItemsStart()->Child(list);
    vertical ? menu->FlexCol() : menu->FlexRow();

    Str documentation = InputCompletionDocumentation(editor);
    if (documentation.len > 0) {
        if (vertical) {
            for (int i = 0; i < documentation.len; i++) {
                if (documentation.s[i] == '\n') {
                    documentation = Str(documentation.s, i);
                    break;
                }
            }
        }
        TextViewStyle textStyle = TextViewStyle::Default();
        textStyle.ParagraphGap(8);
        menu->Child(
            PopoverSurface(cx, Div(a)
                                   ->W(configuredMax)
                                   ->MaxH(240)
                                   ->ClipY()
                                   ->PadX(8)
                                   ->PadY(4))
                ->Child(TextView::New(cx, documentation)
                            ->Font(12)
                            ->Style(textStyle)
                            ->Selectable()
                            ->IntoEl()));
    }
    return Div(a)
        ->Fixed()
        ->Left(x)
        ->Top(y)
        ->OnMouseDownOut(
            ListenTo(view, &InputMenuViewState::CompletionOutside))
        ->Child(menu);
}

CodeActionMenu* CodeActionMenu::New(Ctx* cx, InputState* state) {
    CodeActionMenu* menu = ArenaNew<CodeActionMenu>(cx->a);
    menu->a = cx->a;
    menu->cx = cx;
    menu->state = state;
    return menu;
}

CodeActionMenu* CodeActionMenu::Show(int offset,
                                     const CodeActionItem* items, int n) {
    (void)offset;
    InputPresentCodeActions(state, items, n);
    if (cx->win) AppInvalidate(cx->win);
    return this;
}

void CodeActionMenu::Hide() {
    InputDismissCodeActions(state);
    if (cx && cx->win) AppInvalidate(cx->win);
}

bool CodeActionMenu::HandleAction(InputAction action) {
    return state && InputCodeActionAction(state, cx->app, cx->win, action);
}

El* CodeActionMenu::IntoEl() {
    if (!state || !state->codeActions.open || state->codeActions.items.len <= 0)
        return nullptr;
    const Theme& theme = ThemeNow(cx->app);
    float x = state->caretWinX > 0 ? state->caretWinX - 4.f
                                   : state->inputBounds.x;
    float lineH = state->lastLineH > 0 ? state->lastLineH : 20.f;
    float y = state->caretWinY > 0 ? state->caretWinY + 4.f
                                   : state->inputBounds.y + lineH + 4.f;
    float windowW = cx->win ? WindowSize(cx->win).dipW : 320.f;
    float maxW = windowW > x ? windowW - x : 120.f;
    if (maxW > 320.f) maxW = 320.f;
    if (maxW < 120.f) maxW = 120.f;
    El* list = PopoverSurface(
        cx, Div(a)->FlexCol()->MinW(120)->MaxW(maxW)->MaxH(480)->ClipY()->Pad(
                4));
    Entity<InputMenuViewState> view =
        InputMenuView(cx, state, "code-action-menu");
    for (int i = 0; i < state->codeActions.items.len; i++) {
        bool selected = i == state->codeActions.selected;
        El* row = Div(a)
                      ->FlexRow()
                      ->W(kFill)
                      ->Gap(8)
                      ->Pad(4)
                      ->ItemsCenter()
                      ->Radius(theme.radius * 0.5f)
                      ->Font(12)
                      ->HoverBg(BackgroundOpacity(theme.tokens.accent, 0.8f))
                      ->OnClick(ListenTo(view, &InputMenuViewState::CodeActionClick,
                                         i))
                      ->OnHover(ListenTo(view, &InputMenuViewState::CodeActionHover,
                                         i));
        if (selected) row->Bg(theme.tokens.accent)->Fg(theme.accentFg);
        row->Child(
            TextEl(a, state->codeActions.items[i].title)->LineHeight(1.f));
        list->Child(row);
    }
    return Div(a)
        ->Fixed()
        ->Left(x)
        ->Top(y)
        ->OnMouseDownOut(
            ListenTo(view, &InputMenuViewState::CodeActionOutside))
        ->Child(list);
}

DiagnosticPopover* DiagnosticPopover::New(Ctx* cx, InputState* state,
                                           int diagnostic) {
    DiagnosticPopover* popover = ArenaNew<DiagnosticPopover>(cx->a);
    popover->a = cx->a;
    popover->cx = cx;
    popover->state = state;
    popover->diagnostic = diagnostic;
    return popover;
}

El* DiagnosticPopover::IntoEl() {
    if (!state || diagnostic < 0 || diagnostic >= state->diagnostics.len)
        return nullptr;
    const Theme& theme = ThemeNow(cx->app);
    const Diagnostic& item = state->diagnostics[diagnostic];
    Rgba foreground = theme.blue;
    if (item.severity == DiagnosticSeverity::Error) foreground = theme.red;
    else if (item.severity == DiagnosticSeverity::Warning)
        foreground = theme.yellow;
    else if (item.severity == DiagnosticSeverity::Hint)
        foreground = theme.cyan;
    Rgba background = RgbaMix(theme.background, foreground, 0.8f);
    TextViewStyle textStyle = TextViewStyle::Default();
    textStyle.ParagraphGap(8);
    El* body = TextView::New(cx, item.message)
                   ->Font(12)
                   ->Style(textStyle)
                   ->Selectable()
                   ->IntoEl();
    El* surface = Div(a)
        ->MinW(200)
        ->MaxW(500)
        ->MaxH(320)
        ->ClipY()
        ->PadX(4)
        ->PadY(2)
        ->Radius(theme.radius)
        ->Bg(background)
        ->Fg(foreground)
        ->Border(1, foreground)
        ->BoundsOut(&state->popoverBounds)
        ->Child(body);
    Entity<InputMenuViewState> view =
        InputMenuView(cx, state, "diagnostic-popover");
    surface->OnMouseDownOut(
        ListenTo(view, &InputMenuViewState::PopoverOutside));
    Bounds trigger = state->popoverTriggerBounds;
    if (trigger.w <= 0 || trigger.h <= 0) {
        trigger = {state->hoverDiagnosticX, state->hoverDiagnosticY, 1, 1};
    }
    return Positioner::Side(cx, trigger)
        ->Placement(gpui::Placement::Top)
        ->Align(gpui::Align::Start)
        ->Margin(8)
        ->Child(surface)
        ->IntoEl();
}

HoverPopover* HoverPopover::New(Ctx* cx, InputState* editor,
                                Selection symbolRange, Str hover) {
    HoverPopover* popover = ArenaNew<HoverPopover>(cx->a);
    popover->a = cx->a;
    popover->cx = cx;
    popover->editor = editor;
    popover->symbolRange = symbolRange;
    popover->hover = hover;
    return popover;
}

El* HoverPopover::IntoEl() {
    if (!editor || hover.len <= 0) return nullptr;
    const Theme& theme = ThemeNow(cx->app);
    TextViewStyle textStyle = TextViewStyle::Default();
    textStyle.ParagraphGap(8);
    El* surface = PopoverSurface(
        cx, Div(a)
                ->MinW(200)
                ->MaxW(500)
                ->MaxH(320)
                ->ClipY()
                ->PadX(8)
                ->PadY(4));
    surface->Child(TextView::New(cx, hover)
                       ->Font(12)
                       ->Style(textStyle)
                       ->Selectable()
                       ->IntoEl())
        ->Fg(theme.foreground)
        ->BoundsOut(&editor->popoverBounds);
    Entity<InputMenuViewState> view =
        InputMenuView(cx, editor, "hover-popover");
    surface->OnMouseDownOut(
        ListenTo(view, &InputMenuViewState::PopoverOutside));
    Bounds trigger = editor->popoverTriggerBounds;
    if (trigger.w <= 0 || trigger.h <= 0) {
        trigger = {editor->hoverX, editor->hoverY, 1, 1};
    }
    return Positioner::Side(cx, trigger)
        ->Placement(gpui::Placement::Top)
        ->Align(gpui::Align::Start)
        ->Margin(8)
        ->Child(surface)
        ->IntoEl();
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
    const Theme& th = ThemeNow(cx->app);
    ThemeMode mode = ThemeGet(cx->app);
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

// HighlightTheme::style, for the names a semantic token carries. Rust looks
// the name up in the theme, which holds every tree-sitter capture name;
// this tree's palette is the handful of kinds `syntax.cpp` scans for, so a
// name is mapped onto one of those. A dotted name falls back to its head —
// `keyword.modifier` is a keyword — which is the rule registry.rs applies,
// and a name nothing recognises has no style and is skipped.
static bool SemanticTokColor(Ctx* cx, Str name, Rgba* out) {
    static const struct {
        const char* name;
        SyntaxTok tok;
    } kMap[] = {
        {"keyword", SyntaxTok::Keyword},   {"type", SyntaxTok::Type},
        {"class", SyntaxTok::Type},        {"struct", SyntaxTok::Type},
        {"enum", SyntaxTok::Type},         {"interface", SyntaxTok::Type},
        {"function", SyntaxTok::Function}, {"method", SyntaxTok::Function},
        {"macro", SyntaxTok::Function},    {"property", SyntaxTok::Property},
        {"variable", SyntaxTok::Property}, {"parameter", SyntaxTok::Property},
        {"string", SyntaxTok::String},     {"number", SyntaxTok::Number},
        {"boolean", SyntaxTok::Boolean},   {"comment", SyntaxTok::Comment},
        {"tag", SyntaxTok::Tag},           {"attribute", SyntaxTok::Attribute},
    };
    const Theme& th = ThemeNow(cx->app);
    ThemeMode mode = ThemeGet(cx->app);
    Str head = name;
    for (int pass = 0; pass < 2; pass++) {
        for (const auto& row : kMap) {
            if (base::StrEqI(head, row.name)) {
                *out = SyntaxTokColor(row.tok, mode, th.foreground);
                return true;
            }
        }
        // Try the head of a dotted name once, and then give up.
        int dot = -1;
        for (int i = 0; i < head.len; i++) {
            if (head.s[i] == '.') {
                dot = i;
                break;
            }
        }
        if (dot < 0) {
            break;
        }
        head = Str(head.s, dot);
    }
    return false;
}

// The semantic tokens a provider published, over the document, as runs the
// rows slice out of — the same shape the language's own captures come in.
static int SemanticSpans(Ctx* cx, InputState* state, Str text, TextSpan* out,
                         int cap) {
    if (!state || state->semanticTokens.len == 0 || !text.s) {
        return 0;
    }
    const int kWindow = 1024;
    auto* window =
        (SemanticRange*)Alloc(cx->a, (int)sizeof(SemanticRange) * kWindow);
    if (!window) {
        return 0;
    }
    // Rust windows to what is on screen; the rows here are built from spans
    // over the whole document, so the whole document is the window.
    int n = SemanticTokensForRange(state->semanticTokens.els,
                                   state->semanticTokens.len, text,
                                   Selection{0, text.len}, window, kWindow);
    int m = 0;
    for (int i = 0; i < n && m < cap; i++) {
        Rgba c;
        if (!SemanticTokColor(cx, window[i].name, &c)) {
            continue;
        }
        out[m].lo = window[i].range.start;
        out[m].hi = window[i].range.end;
        out[m].color = c;
        out[m].bg = Rgba8(0, 0, 0, 0);
        out[m].underline = false;
        out[m].wavy = false;
        m++;
    }
    return m;
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
    const Theme& th = ThemeNow(cx->app);
    if (state) {
        state->searchable = searchable;
    }
    InputEditorStyle style;
    style.foreground = th.foreground;
    style.mutedForeground = th.mutedFg;
    style.caret = th.caret;
    style.selection = RgbaOpacity(th.selection, 0.4f);
    // theme.mono_font_size, which is 13 rather than the 12 this drew at: a
    // narrower row is a row that does not soft-wrap where Rust's does. A
    // caller that set a size of its own refines over it, and the rows follow
    // that size — Rust's `line_height(relative(1.5))` on the editor.
    style.fontSize = fontSize > 0 ? fontSize : 13;
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
    // hover_definition_style: Rust takes `link_text` out of the highlight
    // theme, which this tree's scanner palette has no entry for — the UI
    // theme's link colour is the same blue a Link is drawn in.
    style.linkText = th.blue;
    // What a ghost line covers the row under it with.
    style.background = th.background;
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
    // The semantic tokens over the language's own captures. Rust composes
    // the two with `combine_highlights`, which folds the overlapping styles
    // out of a HashSet — so which of them wins where both speak is not
    // defined there. It is here: what the server said wins, which is what
    // the protocol means by layering semantic tokens over a lexer, and it is
    // the half that can tell a type from a variable.
    {
        auto* sem = (TextSpan*)Alloc(a, (int)sizeof(TextSpan) * kMaxSpans);
        int nSem = sem ? SemanticSpans(cx, state, text, sem, kMaxSpans) : 0;
        if (nSem > 0) {
            auto* tmp = (TextSpan*)Alloc(a, (int)sizeof(TextSpan) * kMaxSpans);
            if (tmp) {
                n = MergeDecorations(spans, n, sem, nSem, kMaxSpans, tmp);
            }
        }
    }
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
        scroller = InputBase::New(cx, id, true,
                                  AccessibilityRole::MultilineTextInput)
                       ->BindInput(state)
                       ->FlexCol()
                       ->W(kFill)
                       ->H(h)
                       ->ClipY()
                       ->ScrollY(state ? state->scrollY : 0)
                       ->Child(editor);
    }
    El* completionMenu = CompletionMenu::New(cx, state)->IntoEl();
    if (!completionMenu) {
        completionMenu = CodeActionMenu::New(cx, state)->IntoEl();
    }

    El* diagPopover = nullptr;
    if (state) {
        diagPopover =
            DiagnosticPopover::New(cx, state, state->hoverDiagnostic)->IntoEl();
        if (!diagPopover && state->hoverDiagnostic < 0) {
            diagPopover = HoverPopover::New(cx, state, state->hoverRange,
                                            state->hoverText)
                              ->IntoEl();
        }
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
        // `v_flex().size_full().children(search_panel)` -- the bar is a
        // sibling of the editor under a box that names itself nothing, and
        // upstream's `.id("search-panel")` is a bare constant because the
        // panel is a view of its own and rendering an entity pushes its
        // identity on the stack. The port has no entity to push, so the
        // editor's name is what stands in for it.
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
