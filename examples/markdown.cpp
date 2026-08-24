/* crates/story/examples/markdown.rs — the markdown source on the left, what
   it renders on the right, and a status bar under both.

   The two panes are one `h_resizable` with the editor in the first and a
   TextView over the editor's own text in the second, so every keystroke on
   the left is reparsed and redrawn on the right. The editor is the code
   editor with line numbers, a two-space tab and the find bar on ctrl-f, and
   the fenced blocks in the preview carry the actions the Rust example hangs
   on them: a Clipboard for the code, and a Run button on the two languages
   it knows. Two of its markdown plugins are registered too — a `$SYMBOL`
   paragraph draws as a quote card, and a `<UserCard id=".." />` block as a
   card with an avatar and a Follow button that remembers being pressed.

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
   - The **math plugin** is not registered yet; upstream renders a formula
     through KaTeX in node, which this tree has neither of.
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

// ─── the three plugins the Rust example registers ─────────────────────────

// TickerQuote: what the story hands the plugin, since there is no market
// behind it.
struct TickerQuote {
    const char* symbol;
    const char* name;
    float price;
    float change;
};

static const TickerQuote kQuotes[] = {
    {"AAPL.US", "Apple Inc.", 300.21f, 5.2f},
    {"TSLA.US", "Tesla, Inc.", 412.05f, -2.13f},
};

static const TickerQuote* QuoteFor(Str symbol) {
    for (const TickerQuote& q : kQuotes) {
        Str name = Str(q.symbol);
        if (name.len == symbol.len &&
            memcmp(name.s, symbol.s, (size_t)name.len) == 0) {
            return &q;
        }
    }
    return nullptr;
}

// ticker_symbol: `$` then letters, digits and at least one dot.
static bool TickerSymbol(Str text, Str* out) {
    if (text.len < 2 || text.s[0] != '$') {
        return false;
    }
    Str sym = Str(text.s + 1, text.len - 1);
    bool dot = false;
    for (int i = 0; i < sym.len; i++) {
        char c = sym.s[i];
        bool alnum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                     (c >= '0' && c <= '9');
        if (c == '.') {
            dot = true;
        } else if (!alnum) {
            return false;
        }
    }
    if (!dot) {
        return false;
    }
    *out = sym;
    return true;
}

static bool TickerParse(Ctx* cx, component::MdNode* n, Str text, void*,
                        component::MdPluginNode* out) {
    (void)cx;
    // A paragraph whose one child is text: `[Node::Text(text)]` in Rust.
    if (n->kind != component::MdKind::Paragraph || !n->runFirst ||
        n->runFirst->next) {
        return false;
    }
    Str sym;
    if (!TickerSymbol(text, &sym)) {
        return false;
    }
    out->text = text;
    out->markdown = text;
    out->data = (void*)QuoteFor(sym);
    if (!out->data) {
        // An unknown symbol still draws, the way Rust's `_ =>` arm does.
        static const TickerQuote unknown = {"", "Unknown", 0.f, 0.f};
        out->data = (void*)&unknown;
    }
    return true;
}

static El* TickerRender(Ctx* cx, const component::MdPluginNode* node, void*) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    const auto* q = (const TickerQuote*)node->data;
    bool up = q->change >= 0.f;
    Rgba trend = up ? th.green : th.red;

    El* head = Div(a)->FlexRow()->W(kFill)->ItemsCenter()->JustifyBetween();
    El* names = Div(a)->FlexCol()->Gap(4);
    names->Child(TextEl(a, node->text)->Font(14)->LineHeight(1.f)->Semibold());
    names->Child(
        TextEl(a, Str(q->name))->Font(12)->LineHeight(1.f)->Fg(th.mutedFg));
    head->Child(names);
    El* chip = Div(a)
                   ->FlexRow()
                   ->ItemsCenter()
                   ->Gap(2)
                   ->PadX(4)
                   ->PadY(2)
                   ->Radius(th.radius)
                   ->Bg(RgbaOpacity(trend, 0.12f))
                   ->Fg(trend);
    chip->Child(IconEl(a, up ? IconName::ArrowUp : IconName::ArrowDown, 12));
    chip->Child(TextEl(a, StrDup(a, fmt("%+.1f%%", (double)q->change)))
                    ->Font(12)
                    ->LineHeight(1.f)
                    ->Medium());
    head->Child(chip);

    El* last = Div(a)->FlexRow()->W(kFill)->ItemsCenter()->JustifyBetween();
    last->Child(TextEl(a, StrDup(a, fmt("%.2f", (double)q->price)))
                    ->Font(18)
                    ->LineHeight(1.f)
                    ->Semibold());
    last->Child(
        TextEl(a, StrL("Last"))->Font(12)->LineHeight(1.f)->Fg(th.mutedFg));

    return Div(a)
        ->FlexCol()
        ->W(240)
        ->Gap(6)
        ->PadX(12)
        ->PadY(8)
        ->Radius(th.radius)
        ->Border(1, th.border)
        ->Bg(th.tokens.background)
        ->Child(head)
        ->Child(last);
}

// The two people the example knows, by the id its `<UserCard />` names.
struct UserCardDef {
    const char* id;
    const char* name;
    const char* avatar;
};

static const UserCardDef kUsers[] = {
    {"huacnlee", "Jason Lee",
     "https://avatars.githubusercontent.com/u/5518?v=4"},
    {"madcodelife", "Floyd Wang",
     "https://avatars.githubusercontent.com/u/28998859?v=4"},
};

// html_tag_name / html_attr: the two readings of a raw block the Rust example
// makes, without a regex.
static bool HtmlTagIs(Str raw, const char* name) {
    int at = 0;
    while (at < raw.len && (raw.s[at] == ' ' || raw.s[at] == '\n')) {
        at++;
    }
    if (at >= raw.len || raw.s[at] != '<') {
        return false;
    }
    at++;
    Str want = Str(name);
    if (at + want.len > raw.len ||
        memcmp(raw.s + at, want.s, (size_t)want.len) != 0) {
        return false;
    }
    char after = at + want.len < raw.len ? raw.s[at + want.len] : '\0';
    return after == ' ' || after == '/' || after == '>' || after == '\n';
}

static bool HtmlAttr(Str raw, const char* name, Str* out) {
    char pattern[64];
    int n = 0;
    for (const char* p = name; *p && n < 60; p++) {
        pattern[n++] = *p;
    }
    pattern[n++] = '=';
    pattern[n++] = '"';
    for (int i = 0; i + n <= raw.len; i++) {
        if (memcmp(raw.s + i, pattern, (size_t)n) != 0) {
            continue;
        }
        int start = i + n;
        for (int j = start; j < raw.len; j++) {
            if (raw.s[j] == '"') {
                *out = Str(raw.s + start, j - start);
                return true;
            }
        }
        return false;
    }
    return false;
}

static bool UserCardParse(Ctx* cx, component::MdNode* n, Str text, void*,
                          component::MdPluginNode* out) {
    (void)cx;
    if (n->kind != component::MdKind::Html || !HtmlTagIs(text, "UserCard")) {
        return false;
    }
    Str id;
    if (!HtmlAttr(text, "id", &id)) {
        return false;
    }
    out->text = id;
    out->markdown = text;
    const UserCardDef* found = nullptr;
    for (const UserCardDef& u : kUsers) {
        Str uid = Str(u.id);
        if (uid.len == id.len && memcmp(uid.s, id.s, (size_t)uid.len) == 0) {
            found = &u;
        }
    }
    static const UserCardDef unknown = {"", "Unknown", ""};
    out->data = (void*)(found ? found : &unknown);
    return true;
}

// window.use_keyed_state("user-card-follow-{id}"): the button remembers
// whether it was pressed, and nothing else does.
struct FollowState {
    bool following = false;
    static void OnClick(FollowState* self, Ctx* cx, const ClickEvent*) {
        self->following = !self->following;
        Notify(cx);
    }
};

static El* UserCardRender(Ctx* cx, const component::MdPluginNode* node, void*) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    const auto* u = (const UserCardDef*)node->data;
    Str id = node->text;
    Entity<FollowState> follow = KeyedEntity<FollowState>(
        cx, KeyedKey(HashClickId(id), HashClickId(StrL("user-card-follow"))));
    FollowState* st = follow.Get(cx);
    bool following = st && st->following;

    component::Avatar* av = component::Avatar::New(cx)->Name(Str(u->name));
    if (u->avatar[0]) {
        av->Src(Str(u->avatar));
    }
    return Div(a)
        ->FlexRow()
        ->W(300)
        ->ItemsCenter()
        ->Gap(12)
        ->PadX(12)
        ->PadY(8)
        ->Radius(th.radius)
        ->Border(1, th.border)
        ->Child(av->Size(24)->IntoEl())
        ->Child(
            Div(a)->Flex1()->Child(TextEl(a, Str(u->name))->Font(14)->Medium()))
        ->Child(component::Button::New(cx, StrDup(a, fmt("follow-%s", id)))
                    ->Outline()
                    ->WithSize(UiSize::Small)
                    ->Label(following ? StrL("Following") : StrL("Follow"))
                    ->OnClick(ListenTo(follow, &FollowState::OnClick))
                    ->IntoEl());
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
    // .plugin(TickerPlugin::new(..)).plugin(UserCardPlugin::new())
    tv->Plugin(StrL("ticker"), &TickerParse, &TickerRender);
    tv->Plugin(StrL("user-card"), &UserCardParse, &UserCardRender);
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
