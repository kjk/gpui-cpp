/* crates/story/examples/editor.rs — a file tree beside the code editor, with
   the editor's own switches under it.

   The tree walks the working directory and opens a file into the editor,
   which scans it as whatever its extension names. The status bar carries the
   switches Rust's does — line numbers, soft wrap, indent guides, folding,
   read only — with the caret's `line:column` on the right opening the Go to
   line dialog.

   The diagnostics are the part that differs and says so. Upstream lints the
   open document with the `autocorrect` crate and publishes what it finds
   through an LSP-style store; there is no such crate here, so the same seam
   is fed by a small linter of this example's own — a marker comment, a line
   past a hundred columns, trailing whitespace — one severity each, drawn as
   the wavy underline `element.rs` draws a diagnostic with.

   The completion menu is the store's other half that is here: the items are
   upstream's own `fixtures/completion_items.json`, filtered by the word being
   typed, with the selected item's documentation rendered as markdown beside
   the list, and the hover popover, which answers about the word the pointer
   rests on out of the same items, and the code action menu, which is
   TextConvertor's five ways to rewrite a selection. The colours the document
   names are the last of it: upstream hands the text to the `color-lsp` crate,
   and this scans for the two spellings a source file has — a hex literal and
   an `rgb(...)` call — and paints each behind the text that names it. */

#include "gpui.h"

using namespace gpui;

// The directories a walk does not go into. Rust asks the `autocorrect`
// ignorer, which reads .gitignore; this is the short list that keeps the tree
// to the source.
static const char* const kSkipDirs[] = {
    ".git", ".work", "out", "target", "node_modules", ".vs", ".cache"};

static bool SkipEntry(const char* name) {
    if (!name[0] || name[0] == '.') {
        for (const char* d : kSkipDirs) {
            if (strcmp(name, d) == 0) {
                return true;
            }
        }
        return name[0] == '.';
    }
    for (const char* d : kSkipDirs) {
        if (strcmp(name, d) == 0) {
            return true;
        }
    }
    return false;
}

static const int kMaxDiagnostics = 512;

struct EditorApp {
    Entity<TreeState> tree = {};
    InputState editor;
    InputState goToLine;
    Subscription treeSub = {};
    bool seeded = false;
    bool dialogOpen = false;
    int lastSelected = -1;

    // The switches the status bar carries.
    bool lineNumbers = true;
    bool softWrap = false;
    bool indentGuides = true;
    bool folding = true;
    bool readOnly = false;

    // The file the editor holds, and what the tree said last.
    char openPath[1024] = {};
    Str language = {};

    // The lint's own diagnostics, rebuilt when the document changes.
    Diagnostic* diagnostics = nullptr;
    int nDiagnostics = 0;
    int lintedLen = -1;

    ~EditorApp() {
        for (int i = 0; i < nDiagnostics; i++) {
            StrFree(diagnostics[i].message);
        }
        Free(nullptr, diagnostics);
    }
    static El* Render(EditorApp* self, Ctx* cx);
};

// ─── the tree ─────────────────────────────────────────────────────────────

static void SortDir(DirEntry* e, int n) {
    // Folders first, then by name — an insertion sort over one directory.
    for (int i = 1; i < n; i++) {
        DirEntry key = e[i];
        int j = i - 1;
        while (j >= 0) {
            bool after = e[j].isDir != key.isDir
                             ? (!e[j].isDir && key.isDir)
                             : strcmp(e[j].name, key.name) > 0;
            if (!after) {
                break;
            }
            e[j + 1] = e[j];
            j--;
        }
        e[j + 1] = key;
    }
}

static void LoadDir(TreeState* s, const char* path, int parent, int depth) {
    // One listing per level, on the heap: a static array here would be the
    // same array the level above is still walking.
    const int kMaxEntries = 512;
    DirEntry* found = AllocArray<DirEntry>(kMaxEntries);
    if (!found) {
        return;
    }
    int got = PlatListDir(path, found, kMaxEntries);
    SortDir(found, got);
    for (int i = 0; i < got; i++) {
        if (SkipEntry(found[i].name)) {
            continue;
        }
        char child[1024];
        int wrote = snprintf(child, sizeof(child), "%.*s/%.*s", 500, path,
                             (int)sizeof(found[i].name), found[i].name);
        if (wrote <= 0) {
            continue;
        }
        int ix = TreeAddItem(s, StrDup(Str(child)), StrDup(Str(found[i].name)),
                             parent);
        if (ix < 0) {
            break;
        }
        s->items[ix].folder = found[i].isDir;
        if (found[i].isDir && depth > 0) {
            LoadDir(s, child, ix, depth - 1);
        }
    }
    Free(nullptr, found);
}

// The language a file's extension names, which is what the editor scans it as.
static Str LanguageFor(const char* path) {
    const char* dot = nullptr;
    for (const char* p = path; *p; p++) {
        if (*p == '.') {
            dot = p + 1;
        } else if (*p == '/' || *p == '\\') {
            dot = nullptr;
        }
    }
    return dot ? Str(dot) : Str{};
}

// ─── the lint that stands in for autocorrect ──────────────────────────────

static const char* const kMarkers[] = {"TODO", "FIXME", "XXX", "HACK"};

static void AddDiagnostic(EditorApp* self, int lo, int hi,
                          DiagnosticSeverity severity, Str message) {
    if (self->nDiagnostics >= kMaxDiagnostics) {
        return;
    }
    if (!self->diagnostics) {
        self->diagnostics = AllocArray<Diagnostic>(kMaxDiagnostics);
        if (!self->diagnostics) {
            return;
        }
    }
    Diagnostic& d = self->diagnostics[self->nDiagnostics++];
    d.range.start = lo;
    d.range.end = hi;
    d.severity = severity;
    d.message = StrDup(message);
    d.source = StrL("lint");
}

static void Lint(EditorApp* self) {
    for (int i = 0; i < self->nDiagnostics; i++) {
        StrFree(self->diagnostics[i].message);
    }
    self->nDiagnostics = 0;
    Str text = InputValue(&self->editor);
    self->lintedLen = text.len;

    int lineStart = 0;
    for (int at = 0; at <= text.len; at++) {
        bool eol = at == text.len || text.s[at] == '\n';
        if (!eol) {
            continue;
        }
        int lineEnd = at;
        // A marker comment: a warning where it stands.
        for (int i = lineStart; i < lineEnd; i++) {
            for (const char* m : kMarkers) {
                int len = (int)strlen(m);
                if (i + len <= lineEnd &&
                    memcmp(text.s + i, m, (size_t)len) == 0) {
                    AddDiagnostic(self, i, i + len, DiagnosticSeverity::Warning,
                                  StrL("marker left in the source"));
                    i += len - 1;
                    break;
                }
            }
        }
        // Trailing whitespace, and a line past a hundred columns.
        int end = lineEnd;
        while (end > lineStart &&
               (text.s[end - 1] == ' ' || text.s[end - 1] == '\t')) {
            end--;
        }
        if (end < lineEnd) {
            AddDiagnostic(self, end, lineEnd, DiagnosticSeverity::Hint,
                          StrL("trailing whitespace"));
        }
        if (lineEnd - lineStart > 100) {
            AddDiagnostic(self, lineStart + 100, lineEnd,
                          DiagnosticSeverity::Info,
                          StrL("line is longer than 100 columns"));
        }
        lineStart = at + 1;
    }
}

static void OpenFile(EditorApp* self, const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        return;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    // A file this example will not open in one gulp is one the editor should
    // not be asked to hold either.
    const long kMax = 4 * 1024 * 1024;
    if (size <= 0 || size > kMax) {
        fclose(f);
        return;
    }
    char* buf = (char*)Alloc(nullptr, (int)size + 1);
    if (!buf) {
        fclose(f);
        return;
    }
    size_t got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[got] = 0;
    InputSetValue(&self->editor, Str(buf, (int)got));
    Free(nullptr, buf);
    StrCopyZ(self->openPath, (int)sizeof(self->openPath), path);
    self->language = LanguageFor(path);
    Lint(self);
}

static void OnTreeEvent(EditorApp*, Ctx* cx, const TreeEvent*) {
    Notify(cx);
}

// ─── the completion provider ──────────────────────────────────────────────
//
// `ExampleLspStore`'s own: the items are upstream's
// `fixtures/completion_items.json`, read once, and what the menu shows is the
// ones whose label starts with the word being typed. Rust's provider answers
// a Task and filters with a fuzzy matcher; there is nothing to await here and
// no matcher, so it is a prefix and the answer is immediate.

static const int kMaxItems = 256;
static CompletionItem gItems[kMaxItems];
static int gNItems = 0;
static Arena* gItemArena = nullptr;

// CompletionProvider::resolve_completions — completionItem/resolve. The menu
// asks about the item the selection is on; this looks its documentation up in
// the same table the items came from.
// `additionalTextEdits`: what else accepting an item writes. A name that
// needs an import is the case the protocol has in mind, so `unwrap` brings
// its own line in at the top of the document — the caret's insert and this
// go in together, as one undo step.
static const TextEditItem kUseImport = {{0, 0}, StrL("use std::result;\n")};

static void LoadCompletionItems() {
    if (gNItems > 0) {
        return;
    }
    TempStr json = AssetsLoadTextTemp(StrL("completion_items.json"));
    if (json.len <= 0) {
        return;
    }
    gItemArena = ArenaNew();
    JsonValue* root = JsonParse(gItemArena, json);
    if (!root) {
        return;
    }
    for (const JsonValue* v = root->first; v && gNItems < kMaxItems;
         v = v->next) {
        CompletionItem& item = gItems[gNItems];
        item.label = JsonString(JsonGet(v, "label"));
        item.detail = JsonString(JsonGet(v, "detail"));
        item.documentation = JsonString(JsonGet(v, "documentation"));
        if (item.label.len == 0) {
            continue;
        }
        gNItems++;
    }
}

// HoverProvider: the word under the pointer, looked up in the same items —
// its documentation, or the sentence Rust shows when the item has none.
static Str HoverAt(void*, Str text, int offset) {
    LoadCompletionItems();
    int a = offset, b = offset;
    if (!TextWordRangeAt(text, offset, &a, &b) || a >= b) {
        return Str{};
    }
    Str word(text.s + a, b - a);
    for (int i = 0; i < gNItems; i++) {
        const CompletionItem& item = gItems[i];
        if (item.label.len != word.len ||
            memcmp(item.label.s, word.s, (size_t)word.len) != 0) {
            continue;
        }
        return item.documentation.len > 0 ? item.documentation
                                          : StrL("No documentation available.");
    }
    return Str{};
}

static int CompleteFrom(void*, Str, int, Str query, CompletionItem* out,
                        int cap) {
    LoadCompletionItems();
    int n = 0;
    for (int i = 0; i < gNItems && n < cap; i++) {
        const CompletionItem& item = gItems[i];
        if (query.len > item.label.len) {
            continue;
        }
        if (query.len > 0 &&
            memcmp(item.label.s, query.s, (size_t)query.len) != 0) {
            continue;
        }
        out[n] = item;
        // The items go out *thin*, which is what a server does with a
        // thousand of them: the documentation is left for `resolve` to fill
        // in when one is looked at.
        out[n].documentation = Str{};
        // One item brings an import with it, which is what
        // `additionalTextEdits` is for.
        if (item.label.len == 6 && memcmp(item.label.s, "unwrap", 6) == 0) {
            out[n].additionalEdits = &kUseImport;
            out[n].nAdditionalEdits = 1;
        }
        n++;
    }
    return n;
}

static Str ResolveCompletion(void*, Arena* a, const CompletionItem* item) {
    (void)a;
    LoadCompletionItems();
    for (int i = 0; i < gNItems; i++) {
        if (gItems[i].label.len == item->label.len &&
            memcmp(gItems[i].label.s, item->label.s, (size_t)item->label.len) ==
                0) {
            return gItems[i].documentation;
        }
    }
    return Str{};
}

// CompletionProvider::is_completion_trigger. The rule underneath the provider
// is a word character or `.`; a C++ document wants `:` as well, since a
// member of a namespace is reached through one.
static CompletionTrigger CompletionTriggerAt(void*, Str, int, Str typed) {
    if (typed.len == 0) {
        return CompletionTrigger::Close;
    }
    char c = typed.s[0];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_') {
        return CompletionTrigger::Continue;
    }
    if (c == '.' || c == ':') {
        return CompletionTrigger::Open;
    }
    return CompletionTrigger::Close;
}

// ─── the code action provider ─────────────────────────────────────────────
//
// TextConvertor's, which is the five ways it offers to rewrite whatever is
// selected. Rust wraps each in a WorkspaceEdit against a document URI; a
// field is the one document here, so an action is the range and the text.

static Str CaseMapped(Arena* a, Str src, int which) {
    char* out = (char*)Alloc(a, src.len * 2 + 1);
    int n = 0;
    bool startOfWord = true;
    for (int i = 0; i < src.len; i++) {
        char c = src.s[i];
        bool upper = c >= 'A' && c <= 'Z';
        bool lower = c >= 'a' && c <= 'z';
        char up = lower ? (char)(c - 'a' + 'A') : c;
        char down = upper ? (char)(c - 'A' + 'a') : c;
        switch (which) {
            case 0: // Convert to Uppercase
                out[n++] = up;
                break;
            case 1: // Convert to Lowercase
                out[n++] = down;
                break;
            case 2: // Titleize: every word's first letter
                out[n++] = startOfWord ? up : c;
                break;
            case 3: // Capitalize: the first letter of the whole run
                out[n++] = i == 0 ? up : c;
                break;
            default: // snake_case: an underscore in front of every capital
                if (upper && i != 0) {
                    out[n++] = '_';
                }
                out[n++] = down;
                break;
        }
        startOfWord = c == ' ' || c == '\t' || c == '\n';
    }
    return Str(out, n);
}

// ─── the inline completion provider ───────────────────────────────────────
//
// The ghost text in front of the caret. A real one asks a suggestion engine;
// this one answers for two openings a C++ file has plenty of — `for (` and
// `if (` — so the debounce, the drawing and Tab can all be seen working.
static Str InlineCompletionAt(void*, Arena* a, Str text, int offset) {
    if (offset <= 0 || offset > text.len) {
        return Str{};
    }
    // What was typed up to the caret, back to the start of the line.
    int lineStart = offset;
    while (lineStart > 0 && text.s[lineStart - 1] != '\n') {
        lineStart--;
    }
    Str line(text.s + lineStart, offset - lineStart);
    auto endsWith = [](Str s, const char* suffix) {
        int n = (int)strlen(suffix);
        return s.len >= n && memcmp(s.s + s.len - n, suffix, (size_t)n) == 0;
    };
    if (endsWith(line, "for (")) {
        return StrDup(a, StrL("int i = 0; i < n; i++) {\n}"));
    }
    if (endsWith(line, "if (")) {
        return StrDup(a, StrL("!s) {\n    return;\n}"));
    }
    return Str{};
}

// ─── the range semantic tokens provider ───────────────────────────────────
//
// `MarkerHighlighter` from the Rust markdown example: every TODO / FIXME /
// XXX / HACK / NOTE in the document gets a token type of its own, so each
// renders in a different colour of the theme. The scan is synchronous and the
// answer delta-encoded exactly as a language server sends it — which is the
// point of the exercise, since the decoding is the editor's.
struct Marker {
    const char* word;
    const char* type;
};

static const Marker kSemanticMarkers[] = {
    {"TODO", "keyword"},  {"FIXME", "string"}, {"XXX", "number"},
    {"HACK", "function"}, {"NOTE", "type"},
};
static const int kNMarkers =
    (int)(sizeof(kSemanticMarkers) / sizeof(kSemanticMarkers[0]));

// The legend the `tokenType` of each answer indexes into.
static const Str kMarkerLegend[kNMarkers] = {
    StrL("keyword"),  StrL("string"), StrL("number"),
    StrL("function"), StrL("type"),
};

struct MarkerHit {
    int line;
    int col;
    int len;
    int type;
};

static int SemanticTokensFor(void*, Str text, Selection range,
                             SemanticToken* out, int cap) {
    if (cap <= 0 || !text.s) {
        return 0;
    }
    const int kMaxHits = 512;
    MarkerHit hits[kMaxHits];
    int n = 0;
    for (int t = 0; t < kNMarkers && n < kMaxHits; t++) {
        Str word = Str(kSemanticMarkers[t].word);
        for (int i = range.start; i + word.len <= range.end && n < kMaxHits;
             i++) {
            if (memcmp(text.s + i, word.s, (size_t)word.len) != 0) {
                continue;
            }
            RopePoint p = RopeOffsetToPoint(text, i);
            hits[n].line = p.row;
            hits[n].col = p.column;
            hits[n].len = word.len;
            hits[n].type = t;
            n++;
            i += word.len - 1;
        }
    }
    // Document order, which is what the delta encoding is relative to.
    for (int i = 1; i < n; i++) {
        MarkerHit v = hits[i];
        int j = i - 1;
        while (j >= 0 && (hits[j].line > v.line ||
                          (hits[j].line == v.line && hits[j].col > v.col))) {
            hits[j + 1] = hits[j];
            j--;
        }
        hits[j + 1] = v;
    }
    int m = 0;
    int prevLine = 0, prevCol = 0;
    for (int i = 0; i < n && m < cap; i++) {
        int deltaLine = hits[i].line - prevLine;
        out[m].deltaLine = (uint32_t)deltaLine;
        out[m].deltaStart =
            (uint32_t)(deltaLine == 0 ? hits[i].col - prevCol : hits[i].col);
        out[m].length = (uint32_t)hits[i].len;
        out[m].tokenType = (uint32_t)hits[i].type;
        out[m].tokenModifiers = 0;
        prevLine = hits[i].line;
        prevCol = hits[i].col;
        m++;
    }
    return m;
}

// ─── the definition provider ──────────────────────────────────────────────
//
// `ExampleLspStore`'s own: `Duration` is defined in this document, and the
// std type names have a page on doc.rust-lang.org. Rust answers a Task of
// `LocationLink`s; the same two answers are written straight out here.
struct DocLink {
    const char* name;
    const char* path;
};

static const DocLink kRustDocs[] = {
    {"HashMap", "collections/hash_map/struct.HashMap"},
    {"HashSet", "collections/hash_set/struct.HashSet"},
    {"Arc", "sync/struct.Arc"},
    {"RwLock", "sync/struct.RwLock"},
    {"Duration", "time/struct.Duration"},
};

static int DefinitionsAt(void*, Arena* a, Str text, int offset,
                         DefinitionLink* out, int cap) {
    if (cap <= 0) {
        return 0;
    }
    int wa = offset, wb = offset;
    if (!TextWordRangeAt(text, offset, &wa, &wb) || wa >= wb) {
        return 0;
    }
    Str word(text.s + wa, wb - wa);
    // The one symbol this document defines: the first `Duration` in it, which
    // is where the word is declared.
    if (StrEqI(word, StrL("Duration"))) {
        int at = -1;
        for (int i = 0; i + word.len <= text.len; i++) {
            if (memcmp(text.s + i, word.s, (size_t)word.len) == 0 && i != wa) {
                at = i;
                break;
            }
        }
        if (at >= 0) {
            out[0].origin = {wa, wb};
            out[0].uri = Str{};
            out[0].target = {at, at + word.len};
            return 1;
        }
    }
    for (const DocLink& d : kRustDocs) {
        if (!StrEqI(word, Str(d.name))) {
            continue;
        }
        out[0].origin = {wa, wb};
        out[0].uri = StrDup(
            a, fmt("https://doc.rust-lang.org/std/%s.html", Str(d.path)));
        out[0].target = {};
        return 1;
    }
    return 0;
}

static int CodeActionsFor(void*, Arena* a, Str text, Selection sel,
                          CodeActionItem* out, int cap) {
    if (sel.IsEmpty() || sel.end > text.len) {
        return 0;
    }
    static const char* kTitles[] = {
        "Convert to Uppercase", "Convert to Lowercase",  "Titleize",
        "Capitalize",           "Convert to snake_case",
    };
    Str selected(text.s + sel.start, sel.end - sel.start);
    int n = 0;
    for (int i = 0; i < (int)(sizeof(kTitles) / sizeof(kTitles[0])) && n < cap;
         i++) {
        out[n].title = Str(kTitles[i]);
        out[n].range = sel;
        out[n].newText = CaseMapped(a, selected, i);
        n++;
    }
    // One action that is more than one edit, which is what a WorkspaceEdit
    // carries and what a rename or an extract is made of: the two ends of the
    // selection are written separately, last one first so the first does not
    // move the second.
    if (n < cap) {
        auto* edits = (TextEditItem*)Alloc(a, (int)sizeof(TextEditItem) * 2);
        if (edits) {
            edits[0].range = Selection{sel.end, sel.end};
            edits[0].newText = StrL(")");
            edits[1].range = Selection{sel.start, sel.start};
            edits[1].newText = StrL("(");
            out[n].title = StrL("Wrap in Parentheses");
            out[n].edits = edits;
            out[n].nEdits = 2;
            n++;
        }
    }
    return n;
}

// ─── the document colour provider ─────────────────────────────────────────
//
// ExampleLspStore's, which hands the document to the `color-lsp` crate and
// answers what it found. There is no such crate here, so this is the same
// scan over the two spellings that turn up in the source a code editor is
// pointed at: `#rgb` / `#rgba` / `#rrggbb` / `#rrggbbaa`, and `rgb(...)` /
// `rgba(...)` with a number per channel. What comes back is painted behind
// the text that names it, which is what element.rs does with a colour.

static int HexDigit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

// A hex colour at `at`, or 0 if what stands there is not one.
static int HexColorAt(Str text, int at, Rgba* out) {
    int n = 0;
    while (at + 1 + n < text.len && n < 9 &&
           HexDigit(text.s[at + 1 + n]) >= 0) {
        n++;
    }
    if (n != 3 && n != 4 && n != 6 && n != 8) {
        return 0;
    }
    int v[8] = {};
    for (int i = 0; i < n; i++) {
        v[i] = HexDigit(text.s[at + 1 + i]);
    }
    uint8_t c[4] = {0, 0, 0, 255};
    if (n <= 4) {
        // The short spelling doubles each digit: #1af is #11aaff.
        for (int i = 0; i < n; i++) {
            c[i] = (uint8_t)(v[i] * 17);
        }
    } else {
        for (int i = 0; i < n / 2; i++) {
            c[i] = (uint8_t)(v[i * 2] * 16 + v[i * 2 + 1]);
        }
    }
    *out = Rgba{c[0], c[1], c[2], c[3]};
    return n + 1;
}

// `rgb(1, 2, 3)` or `rgba(1, 2, 3, 0.5)`, in as many spellings as a scan this
// small can take: the numbers, in order, and whatever separates them.
static int RgbColorAt(Str text, int at, Rgba* out) {
    bool hasAlpha = at + 4 < text.len && text.s[at + 3] == 'a';
    int i = at + (hasAlpha ? 4 : 3);
    if (i >= text.len || text.s[i] != '(') {
        return 0;
    }
    i++;
    float ch[4] = {0, 0, 0, 1};
    int got = 0;
    while (i < text.len && got < 4) {
        while (i < text.len && (text.s[i] == ' ' || text.s[i] == ',')) {
            i++;
        }
        if (i >= text.len || text.s[i] == ')') {
            break;
        }
        int digits = 0;
        float value = 0;
        while (i < text.len && text.s[i] >= '0' && text.s[i] <= '9') {
            value = value * 10 + (float)(text.s[i] - '0');
            i++;
            digits++;
        }
        if (i < text.len && text.s[i] == '.') {
            i++;
            float scale = 0.1f;
            while (i < text.len && text.s[i] >= '0' && text.s[i] <= '9') {
                value += (float)(text.s[i] - '0') * scale;
                scale *= 0.1f;
                i++;
                digits++;
            }
        }
        if (digits == 0) {
            return 0;
        }
        ch[got++] = value;
    }
    if (i >= text.len || text.s[i] != ')' || got < 3) {
        return 0;
    }
    auto clamp = [](float v) {
        return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
    };
    *out = Rgba{clamp(ch[0]), clamp(ch[1]), clamp(ch[2]),
                clamp(ch[3] <= 1.f ? ch[3] * 255.f : ch[3])};
    return i + 1 - at;
}

static bool WordCharAt(Str text, int at) {
    if (at < 0 || at >= text.len) {
        return false;
    }
    char c = text.s[at];
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

static int DocumentColorsIn(void*, Str text, DocumentColor* out, int cap) {
    int n = 0;
    for (int i = 0; i < text.len && n < cap; i++) {
        Rgba color = {};
        int len = 0;
        if (text.s[i] == '#') {
            len = HexColorAt(text, i, &color);
        } else if ((text.s[i] == 'r' && i + 3 < text.len &&
                    memcmp(text.s + i, "rgb", 3) == 0) &&
                   !WordCharAt(text, i - 1)) {
            len = RgbColorAt(text, i, &color);
        }
        if (len <= 0) {
            continue;
        }
        out[n].range = Selection{i, i + len};
        out[n].color = color;
        n++;
        i += len - 1;
    }
    return n;
}

// ─── the status bar's switches ────────────────────────────────────────────

enum {
    kToggleLineNumbers = 0,
    kToggleSoftWrap,
    kToggleIndentGuides,
    kToggleFolding,
    kToggleReadOnly
};

static void OnToggle(EditorApp* self, Ctx* cx, const ClickEvent*,
                     intptr_t which) {
    switch (which) {
        case kToggleLineNumbers:
            self->lineNumbers = !self->lineNumbers;
            self->editor.mode.lineNumber = self->lineNumbers;
            break;
        case kToggleSoftWrap:
            self->softWrap = !self->softWrap;
            self->editor.softWrap = self->softWrap;
            break;
        case kToggleIndentGuides:
            self->indentGuides = !self->indentGuides;
            break;
        case kToggleFolding:
            self->folding = !self->folding;
            self->editor.mode.folding = self->folding;
            break;
        default:
            self->readOnly = !self->readOnly;
            self->editor.readonly = self->readOnly;
            break;
    }
    Notify(cx);
}

static void OpenGoTo(EditorApp* self, Ctx* cx, const ClickEvent*) {
    RopePoint at = InputCursorPosition(&self->editor);
    InputSetPlaceholder(&self->goToLine,
                        StrDup(fmt("%d:%d", at.row, at.column)));
    InputSetValue(&self->goToLine, Str{});
    self->goToLine.focused = true;
    self->dialogOpen = true;
    Notify(cx);
}

static void CloseGoTo(EditorApp* self, Ctx* cx, const ClickEvent*) {
    self->dialogOpen = false;
    self->goToLine.focused = false;
    self->editor.focused = true;
    Notify(cx);
}

static void ConfirmGoTo(EditorApp* self, Ctx* cx, const ClickEvent* ev) {
    Str query = InputValue(&self->goToLine);
    int line = 0;
    int column = 1;
    int at = 0;
    bool any = false;
    while (at < query.len && query.s[at] >= '0' && query.s[at] <= '9') {
        line = line * 10 + (query.s[at] - '0');
        at++;
        any = true;
    }
    if (any && at < query.len && query.s[at] == ':') {
        at++;
        column = 0;
        while (at < query.len && query.s[at] >= '0' && query.s[at] <= '9') {
            column = column * 10 + (query.s[at] - '0');
            at++;
        }
    }
    if (any) {
        Str text = InputValue(&self->editor);
        int row = line > 0 ? line - 1 : 0;
        int col = column > 0 ? column - 1 : 0;
        int offset = RopeLineStartOffset(text, row) + col;
        InputMoveTo(&self->editor, cx,
                    RopeClipOffset(text, offset, Bias::Left));
    }
    CloseGoTo(self, cx, ev);
}

static El* ToggleButton(Ctx* cx, Str id, Str label, bool on, Listener toggle,
                        intptr_t which) {
    return component::Button::New(cx, id)
        ->Ghost()
        ->WithSize(UiSize::XSmall)
        ->Label(label)
        ->Selected(on)
        ->OnClick(ListenerArg(toggle, which))
        ->IntoEl();
}

El* EditorApp::Render(EditorApp* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    if (!self->seeded) {
        self->seeded = true;
        self->tree = EntityNewState<TreeState>(cx->app);
        if (TreeState* s = self->tree.Get(cx)) {
            LoadDir(s, ".", -1, 2);
            TreeRebuild(s);
            self->treeSub = Subscribe(cx, self->tree, &OnTreeEvent);
        }
    }
    // The document is linted when it changes; Rust's store does it on every
    // InputEvent and publishes what it found.
    if (self->lintedLen != InputValue(&self->editor).len) {
        Lint(self);
    }
    cx->win->input = self->dialogOpen ? &self->goToLine : &self->editor;

    WinSize win = WindowSize(cx->win);
    float bodyH = win.dipH - 30;

    // The tree reports a selection rather than a click; a row that names a
    // file and is not the one already open is the one to read in.
    if (TreeState* s = self->tree.Get(cx)) {
        if (s->selected != self->lastSelected) {
            self->lastSelected = s->selected;
            const TreeItem* item = TreeEntryItem(s, s->selected);
            if (item && !item->folder && item->id.s) {
                char path[1024];
                StrCopyZ(path, (int)sizeof(path), item->id.s);
                OpenFile(self, path);
            }
        }
    }
    El* tree =
        component::Tree::New(cx, StrL("files"), self->tree)->H(bodyH)->IntoEl();
    El* left = Div(a)
                   ->FlexCol()
                   ->W(240)
                   ->H(bodyH)
                   ->Shrink0()
                   ->BorderR(1, th.border)
                   ->Child(tree);

    component::Highlighter* ed =
        component::Highlighter::New(cx, StrL("editor"), &self->editor);
    ed->H(bodyH)->ActiveLine();
    if (self->language.len > 0) {
        ed->Language(self->language);
    }
    if (self->indentGuides) {
        ed->IndentGuides();
    }
    if (self->folding) {
        ed->Folding();
    }
    ed->Diagnostics(self->diagnostics, self->nDiagnostics);
    El* right = Div(a)->FlexCol()->Flex1()->H(bodyH)->Child(ed->IntoEl());

    El* body = Div(a)->FlexRow()->W(kFill)->H(bodyH)->Child(left)->Child(right);

    Listener toggle = Listen(cx, &OnToggle);
    component::StatusBar* bar = component::StatusBar::New(cx);
    bar->Left(ToggleButton(cx, StrL("line-number"), StrL("Line Number"),
                           self->lineNumbers, toggle, kToggleLineNumbers));
    bar->Left(ToggleButton(cx, StrL("soft-wrap"), StrL("Soft Wrap"),
                           self->softWrap, toggle, kToggleSoftWrap));
    bar->Left(ToggleButton(cx, StrL("indent-guides"), StrL("Indent Guides"),
                           self->indentGuides, toggle, kToggleIndentGuides));
    bar->Left(ToggleButton(cx, StrL("folding"), StrL("Folding"), self->folding,
                           toggle, kToggleFolding));
    bar->Left(ToggleButton(cx, StrL("readonly"), StrL("Read Only"),
                           self->readOnly, toggle, kToggleReadOnly));
    RopePoint at = InputCursorPosition(&self->editor);
    bar->Right(component::Button::New(cx, StrL("line-column"))
                   ->Ghost()
                   ->WithSize(UiSize::XSmall)
                   ->Label(StrDup(a, fmt("%d:%d", at.row, at.column)))
                   ->OnClick(Listen(cx, &OpenGoTo))
                   ->IntoEl());

    El* root = Div(a)
                   ->FlexCol()
                   ->SizeFull()
                   ->Bg(th.tokens.background)
                   ->Child(body)
                   ->Child(bar->IntoEl());
    if (self->dialogOpen) {
        root->Child(component::Dialog::New(cx)
                        ->Open(true)
                        ->Title(StrL("Go to line"))
                        ->Body(component::Input::New(cx, StrL("go-to-line"),
                                                     &self->goToLine)
                                   ->IntoEl())
                        ->Confirm()
                        ->OnClose(Listen(cx, &CloseGoTo))
                        ->OnCancel(Listen(cx, &CloseGoTo))
                        ->OnOk(Listen(cx, &ConfirmGoTo))
                        ->IntoEl(win));
    }
    return root;
}

int GpuiMain(int argc, char** argv) {
    (void)argc;
    (void)argv;
    App* app = AppNew();
    AssetsClear();
    AssetsAddDefaultRoots(StrL("editor"));
    Entity<EditorApp> view = EntityNew<EditorApp>(app);
    EditorApp* self = view.Get(app);
    self->editor.kind = InputKind::Editor;
    self->editor.mode.kind = LayoutModeKind::CodeEditor;
    self->editor.mode.tabSize = 4;
    self->editor.mode.lineNumber = true;
    self->editor.mode.folding = true;
    InputSetPlaceholder(&self->editor, StrL("Open a file from the tree..."));
    // The completion provider, which is what makes the menu open as a word is
    // typed and on `.`.
    self->editor.completionProvider = &CompleteFrom;
    // The two halves of the completion surface beside it: when the menu
    // opens, and where an item's documentation comes from once it is looked
    // at.
    self->editor.completionTrigger = &CompletionTriggerAt;
    self->editor.completionResolve = &ResolveCompletion;
    // And the hover provider, which answers about the word the pointer rests
    // on out of the same items.
    self->editor.hoverProvider = &HoverAt;
    // And the code actions, which ctrl-. offers over a selection.
    self->editor.codeActionProvider = &CodeActionsFor;
    // And the colours the document names, painted where they are named.
    self->editor.documentColorProvider = &DocumentColorsIn;
    // And where a symbol is defined: ctrl-hover underlines one it can reach,
    // ctrl-click follows it. `Duration` is defined in this document; the
    // other four std names open their page on doc.rust-lang.org.
    self->editor.definitionProvider = &DefinitionsAt;
    // And what a language server would say about the document beyond what the
    // scanner can see — here the five markers, each in its own colour, over
    // the highlighting rather than instead of it.
    // And the ghost text in front of the caret, which Tab accepts.
    self->editor.inlineCompletionProvider = &InlineCompletionAt;
    self->editor.semanticTokensProvider = &SemanticTokensFor;
    self->editor.semanticLegend = kMarkerLegend;
    self->editor.nSemanticLegend = kNMarkers;
    // The file the example opens with, which is its own source.
    OpenFile(self, "examples/editor.cpp");
    self->editor.focused = true;
    Window* win = WindowOpenView(app, StrL("Editor Example"), 1280, 900,
                                 view.id, WinOpts{});
    (void)win;
    int rc = AppRun(app);
    AppFree(app);
    return rc;
}
