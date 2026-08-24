/* crates/story/examples/markdown.rs — the markdown source on the left, what
   it renders on the right, and a status bar under both.

   The two panes are one `h_resizable` with the editor in the first and a
   TextView over the editor's own text in the second, so every keystroke on
   the left is reparsed and redrawn on the right. The editor is the code
   editor with line numbers, a two-space tab and the find bar on ctrl-f, and
   the fenced blocks in the preview carry the actions the Rust example hangs
   on them: a Clipboard for the code, and a Run button on the two languages
   it knows.

   Rust marks TODO / FIXME / XXX / HACK / NOTE in the source through an
   LSP-style `DocumentRangeSemanticTokensProvider`. There is no language
   server here, so the same marks are found by scanning the text and handed to
   the editor as a decoration collection — the seam `create_decorations_
   collection` uses, and the colours are the same five syntax token types
   upstream maps the markers to.

   What this does not have, and why:

   - The **Open** action wants a file dialog, which this tree does not have;
     the document is the fixture that upstream's `include_str!` bakes in.
   - **Selection: Plain / Source** would have to map a selection in the
     rendered document back to the markdown that produced it, and an MdNode
     here carries no source offsets.
   - **Table: Scroll / Wrap** — every table in this tree wraps, the way
     `render_wrap_table` does; the measured-width scrolling layout upstream
     defaults to is not ported.
   - The three **markdown plugins** (ticker, user card, math) want a plugin
     seam on TextView; the math one wants KaTeX through node besides.
   - The source is drawn unhighlighted: `syntax.cpp` has a scanner for ten
     languages and markdown is not yet one of them. */

#include "gpui.h"

using namespace gpui;

// MARKERS, and the highlight-theme token type each is drawn as.
struct MarkerDef {
    const char* word;
    component::SyntaxTok tok;
};

static const MarkerDef kMarkers[] = {
    {"TODO", component::SyntaxTok::Keyword},
    {"FIXME", component::SyntaxTok::String},
    {"XXX", component::SyntaxTok::Number},
    {"HACK", component::SyntaxTok::Function},
    {"NOTE", component::SyntaxTok::Type},
};
static const int kMarkerCount = (int)(sizeof(kMarkers) / sizeof(kMarkers[0]));

// Room for the markers one document can hold; the fixture has a handful.
static const int kMaxMarkers = 256;

struct MarkdownApp {
    InputState source;
    Entity<component::ResizableState> split = {};
    float previewScroll = 0;
    // The last link the preview reported, shown in the status bar — Rust
    // prints it and opens the URL; opening a browser mid-demo is not what a
    // screenshot wants, so this says the handler ran instead.
    char lastLink[512] = {};
    bool seeded = false;

    static El* Render(MarkdownApp* self, Ctx* cx);
};

// memcmp over the document, from `from`, since the editor's text is not a
// C string and the markers are plain words.
static int FindFrom(Str hay, Str needle, int from) {
    for (int i = from; i + needle.len <= hay.len; i++) {
        if (memcmp(hay.s + i, needle.s, (size_t)needle.len) == 0) {
            return i;
        }
    }
    return -1;
}

// The provider's job, without the provider: every marker word in the text,
// as a decoration in the colour its token type paints.
static int FindMarkers(Ctx* cx, Str text, TextSpan* out, int cap) {
    const Theme& th = cx->theme();
    ThemeMode mode = ThemeGet();
    int n = 0;
    for (int i = 0; i < kMarkerCount && n < cap; i++) {
        Str word = Str(kMarkers[i].word);
        int at = 0;
        while (n < cap) {
            int lo = FindFrom(text, word, at);
            if (lo < 0) {
                break;
            }
            out[n].lo = lo;
            out[n].hi = lo + word.len;
            out[n].color =
                component::SyntaxTokColor(kMarkers[i].tok, mode, th.foreground);
            out[n].bg = Rgba8(0, 0, 0, 0);
            out[n].underline = false;
            n++;
            at = lo + word.len;
        }
    }
    // The decorations arrive in document order, which is what the merge with
    // the language's own captures walks both lists in — and what a semantic
    // tokens response is, since it is delta-encoded from the one before it.
    for (int i = 1; i < n; i++) {
        TextSpan sp = out[i];
        int j = i - 1;
        while (j >= 0 && out[j].lo > sp.lo) {
            out[j + 1] = out[j];
            j--;
        }
        out[j + 1] = sp;
    }
    return n;
}

static void OnLink(MarkdownApp* self, Ctx* cx, const ClickEvent*,
                   intptr_t href) {
    StrCopyZ(self->lastLink, (int)sizeof(self->lastLink),
             href ? (const char*)href : "");
    Notify(cx);
}

static void OnPreviewScroll(MarkdownApp* self, Ctx* cx, const ScrollEvent* ev) {
    self->previewScroll = ev->offsetY;
    Notify(cx);
}

// The two languages the example offers a Run button for.
static const char* const kRunnable[] = {"rust", "python"};

static void OnRunCode(MarkdownApp* self, Ctx* cx, const ClickEvent*,
                      intptr_t which) {
    // `println!("Running {} code: {}", lang, code)` — the example's own
    // placeholder for a terminal, which this tree does not have either.
    Str lang = Str(kRunnable[which == 1 ? 1 : 0]);
    TempStr msg = fmt("Running %s code", lang);
    StrCopyZ(self->lastLink, (int)sizeof(self->lastLink), msg.s);
    Notify(cx);
}

// code_block_actions: a Clipboard over the block's text, and a Run button on
// the two languages the Rust example offers one for.
static El* CodeActions(Ctx* cx, void* data, Str code, Str lang) {
    (void)data;
    Arena* a = cx->a;
    El* row = Div(a)->FlexRow()->Gap(4)->ItemsCenter();
    row->Child(component::Clipboard::New(cx, StrL("copy"))
                   ->Value(StrDup(a, code))
                   ->IntoEl());
    int runnable = -1;
    for (int i = 0; i < 2; i++) {
        Str name = Str(kRunnable[i]);
        if (lang.len == name.len &&
            memcmp(lang.s, name.s, (size_t)name.len) == 0) {
            runnable = i;
        }
    }
    if (runnable >= 0) {
        row->Child(component::Button::New(cx, StrL("run-terminal"))
                       ->Icon(IconName::SquareTerminal)
                       ->Ghost()
                       ->WithSize(UiSize::XSmall)
                       ->OnClick(ListenerArg(Listen(cx, &OnRunCode), runnable))
                       ->IntoEl());
    }
    return row;
}

El* MarkdownApp::Render(MarkdownApp* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    if (!self->seeded) {
        self->seeded = true;
        self->split = EntityNewState<component::ResizableState>(cx->app);
    }
    // The editor holds the document; the preview reads it back every frame,
    // which is what makes a keystroke on the left redraw the right.
    Str text = InputValue(&self->source);
    cx->win->input = &self->source;

    auto* marks = (TextSpan*)Alloc(a, (int)sizeof(TextSpan) * kMaxMarkers);
    int nMarks = FindMarkers(cx, text, marks, kMaxMarkers);

    // `Editor::new(&state).h(relative(1.))`: the editor fills its panel. A
    // Highlighter takes pixels, so the panel's height is the window's less
    // the status bar under it -- py_1 over a text_sm line, so 26.
    float editorH = WindowSize(cx->win).dipH - 26;
    component::Highlighter* ed =
        component::Highlighter::New(cx, StrL("source"), &self->source);
    ed->H(editorH)->Language(StrL("markdown"))->Decorations(marks, nMarks);
    El* left = Div(a)->FlexCol()->SizeFull()->Child(ed->IntoEl());

    component::TextView* tv = component::TextView::New(cx, text);
    El* preview = tv->Selectable()
                      ->OnLink(Listen(cx, &OnLink))
                      ->CodeBlockActions(&CodeActions, self)
                      ->IntoEl();
    // `.p_5().scrollable(true)`: the document scrolls inside its own panel.
    El* right =
        Div(a)
            ->FlexCol()
            ->SizeFull()
            ->ClipY()
            ->ScrollY(self->previewScroll)
            ->ScrollId(HashClickId(StrL("preview")))
            ->OnScroll(Listen(cx, &OnPreviewScroll))
            ->Child(Div(a)->FlexCol()->W(kFill)->Pad(20)->Child(preview));

    El* split = component::Resizable::New(cx, StrL("container"), self->split)
                    ->H(kFill)
                    ->Panel(left, 520, 200)
                    ->Grow(right, 200)
                    ->IntoEl();

    component::StatusBar* bar = component::StatusBar::New(cx);
    if (self->lastLink[0]) {
        bar->Left(Str(self->lastLink));
    }

    return Div(a)
        ->FlexCol()
        ->SizeFull()
        ->Bg(th.tokens.background)
        ->Child(Div(a)->Flex1()->W(kFill)->ClipY()->Child(split))
        ->Child(bar->IntoEl());
}

int GpuiMain(int argc, char** argv) {
    (void)argc;
    (void)argv;
    App* app = AppNew();
    AssetsClear();
    AssetsAddDefaultRoots(StrL("markdown"));
    AssetsAddRoot(StrL("assets/markdown"));
    Entity<MarkdownApp> view = EntityNew<MarkdownApp>(app);
    MarkdownApp* self = view.Get(app);
    // EditorState::new(..).language(Markdown).line_number(true).tab_size(2)
    // .searchable(true).placeholder(..).default_value(EXAMPLE)
    // EditorState is InputKind::Editor — a single-line Input drops the
    // newlines a value hands it, which turns a document into one long line.
    self->source.kind = InputKind::Editor;
    self->source.mode.kind = LayoutModeKind::CodeEditor;
    InputSetPlaceholder(&self->source, StrL("Enter your Markdown here..."));
    self->source.mode.tabSize = 2;
    self->source.mode.lineNumber = true;
    TempStr md = AssetsLoadTextTemp(StrL("test.md"));
    InputSetValue(&self->source, md);
    self->source.focused = true;
    Window* win =
        WindowOpenView(app, StrL("Markdown"), 1200, 900, view.id, WinOpts{});
    (void)win;
    int rc = AppRun(app);
    AppFree(app);
    return rc;
}
