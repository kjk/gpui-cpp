#include "component/Text.h"

#include "md4c.h"

namespace gpui {

namespace component {

// ─── parse ────────────────────────────────────────────────────────────────
//
// md4c is a SAX parser: it announces blocks and spans as it walks the source
// and never builds a tree. These callbacks build the one node.rs describes.

struct MdBuild {
    Arena* a = nullptr;
    MdNode* cur = nullptr;
    // The marks in effect, from the enclosing MD_SPAN_* stack.
    uint8_t marks = 0;
    Str href = {};
    // MD_BLOCK_THEAD is not a node of its own; it just marks the rows in it.
    bool inHead = false;
};

static MdNode* Push(MdBuild* b, MdKind k) {
    MdNode* n = ArenaNew<MdNode>(b->a);
    n->kind = k;
    n->parent = b->cur;
    if (b->cur->last) {
        b->cur->last->next = n;
    } else {
        b->cur->first = n;
    }
    b->cur->last = n;
    b->cur = n;
    return n;
}

static void Pop(MdBuild* b) {
    if (b->cur->parent) {
        b->cur = b->cur->parent;
    }
}

// Appends to the run being built when the marks match and the text is the
// next byte of the source; otherwise starts a new run. Most of a paragraph is
// one uninterrupted stretch of source, so this usually collapses to one run
// pointing straight at `source` with nothing copied.
static void AddText(MdBuild* b, Str s) {
    if (s.len <= 0) {
        return;
    }
    MdNode* n = b->cur;
    MdRun* r = n->runLast;
    if (r && r->marks == b->marks && r->href.s == b->href.s &&
        r->text.s + r->text.len == s.s) {
        r->text.len += s.len;
        return;
    }
    r = ArenaNew<MdRun>(b->a);
    r->text = s;
    r->marks = b->marks;
    r->href = b->href;
    if (n->runLast) {
        n->runLast->next = r;
    } else {
        n->runFirst = r;
    }
    n->runLast = r;
}

static int Utf8Encode(char* out, uint32_t cp) {
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xc0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3f));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xe0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
        out[2] = (char)(0x80 | (cp & 0x3f));
        return 3;
    }
    out[0] = (char)(0xf0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3f));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3f));
    out[3] = (char)(0x80 | (cp & 0x3f));
    return 4;
}

struct NamedEntity {
    const char* name;
    uint32_t cp;
};

// md4c hands entities over verbatim — it deliberately keeps no table of them.
// This is the handful that shows up in prose; anything else is left as typed.
static const NamedEntity kEntities[] = {
    {"amp", '&'},      {"lt", '<'},        {"gt", '>'},
    {"quot", '"'},     {"apos", '\''},     {"nbsp", 0xa0},
    {"copy", 0xa9},    {"reg", 0xae},      {"trade", 0x2122},
    {"deg", 0xb0},     {"hellip", 0x2026}, {"mdash", 0x2014},
    {"ndash", 0x2013}, {"lsquo", 0x2018},  {"rsquo", 0x2019},
    {"ldquo", 0x201c}, {"rdquo", 0x201d},  {"bull", 0x2022},
    {"middot", 0xb7},  {"times", 0xd7},    {"rarr", 0x2192},
    {"larr", 0x2190},  {"check", 0x2713},  {"dagger", 0x2020},
};

static uint32_t ParseHex(Str s) {
    uint32_t v = 0;
    for (int i = 0; i < s.len; i++) {
        char c = s.s[i];
        int d;
        if (c >= '0' && c <= '9') {
            d = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            d = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            d = c - 'A' + 10;
        } else {
            return 0;
        }
        v = v * 16 + (uint32_t)d;
    }
    return v;
}

static uint32_t ParseDec(Str s) {
    uint32_t v = 0;
    for (int i = 0; i < s.len; i++) {
        if (s.s[i] < '0' || s.s[i] > '9') {
            return 0;
        }
        v = v * 10 + (uint32_t)(s.s[i] - '0');
    }
    return v;
}

// "&amp;" -> "&". Returns the entity unchanged when it is not one we know.
static Str DecodeEntity(Arena* a, Str e) {
    if (e.len < 3 || e.s[0] != '&' || e.s[e.len - 1] != ';') {
        return e;
    }
    Str body((char*)e.s + 1, e.len - 2);
    uint32_t cp = 0;
    if (body.len > 1 && body.s[0] == '#') {
        if (body.s[1] == 'x' || body.s[1] == 'X') {
            cp = ParseHex(Str((char*)body.s + 2, body.len - 2));
        } else {
            cp = ParseDec(Str((char*)body.s + 1, body.len - 1));
        }
    } else {
        for (const NamedEntity& ne : kEntities) {
            int n = (int)strlen(ne.name);
            if (n == body.len && memcmp(ne.name, body.s, (size_t)n) == 0) {
                cp = ne.cp;
                break;
            }
        }
    }
    if (cp == 0) {
        return e;
    }
    char* out = (char*)Alloc(a, 5);
    if (!out) {
        return e;
    }
    int n = Utf8Encode(out, cp);
    out[n] = 0;
    return Str(out, n);
}

// MD_ATTRIBUTE carries entity substrings, which the callers here only ever
// show verbatim, so the raw text is enough.
static Str Attr(Arena* a, const MD_ATTRIBUTE* at) {
    if (!at || !at->text || at->size == 0) {
        return {};
    }
    return StrDup(a, Str((char*)at->text, (int)at->size));
}

static int OnEnterBlock(MD_BLOCKTYPE type, void* detail, void* ud) {
    MdBuild* b = (MdBuild*)ud;
    switch (type) {
        case MD_BLOCK_QUOTE:
            Push(b, MdKind::Quote);
            break;
        case MD_BLOCK_UL:
            Push(b, MdKind::List);
            break;
        case MD_BLOCK_OL: {
            MD_BLOCK_OL_DETAIL* d = (MD_BLOCK_OL_DETAIL*)detail;
            MdNode* n = Push(b, MdKind::List);
            n->ordered = true;
            n->start = d ? (int)d->start : 1;
            break;
        }
        case MD_BLOCK_LI:
            Push(b, MdKind::Item);
            break;
        case MD_BLOCK_HR:
            Push(b, MdKind::Rule);
            break;
        case MD_BLOCK_H: {
            MD_BLOCK_H_DETAIL* d = (MD_BLOCK_H_DETAIL*)detail;
            MdNode* n = Push(b, MdKind::Heading);
            n->level = d ? (uint8_t)d->level : 1;
            break;
        }
        case MD_BLOCK_CODE: {
            MD_BLOCK_CODE_DETAIL* d = (MD_BLOCK_CODE_DETAIL*)detail;
            MdNode* n = Push(b, MdKind::Code);
            n->lang = d ? Attr(b->a, &d->lang) : Str{};
            break;
        }
        case MD_BLOCK_HTML:
            Push(b, MdKind::Html);
            break;
        case MD_BLOCK_P:
            Push(b, MdKind::Paragraph);
            break;
        case MD_BLOCK_TABLE:
            Push(b, MdKind::Table);
            break;
        case MD_BLOCK_THEAD:
            b->inHead = true;
            break;
        case MD_BLOCK_TBODY:
            b->inHead = false;
            break;
        case MD_BLOCK_TR: {
            MdNode* n = Push(b, MdKind::Row);
            n->head = b->inHead;
            break;
        }
        case MD_BLOCK_TH:
        case MD_BLOCK_TD: {
            MD_BLOCK_TD_DETAIL* d = (MD_BLOCK_TD_DETAIL*)detail;
            MdNode* n = Push(b, MdKind::Cell);
            n->align = d ? (uint8_t)d->align : 0;
            break;
        }
        default:
            break;
    }
    return 0;
}

static int OnLeaveBlock(MD_BLOCKTYPE type, void* detail, void* ud) {
    (void)detail;
    MdBuild* b = (MdBuild*)ud;
    switch (type) {
        case MD_BLOCK_DOC:
        case MD_BLOCK_THEAD:
        case MD_BLOCK_TBODY:
            break;
        default:
            Pop(b);
            break;
    }
    return 0;
}

static uint8_t SpanMark(MD_SPANTYPE type) {
    switch (type) {
        case MD_SPAN_EM:
            return MdItalic;
        case MD_SPAN_STRONG:
            return MdBold;
        case MD_SPAN_CODE:
            return MdCode;
        case MD_SPAN_DEL:
            return MdDel;
        case MD_SPAN_U:
            return MdUnderline;
        case MD_SPAN_A:
        case MD_SPAN_WIKILINK:
            return MdLink;
        default:
            return 0;
    }
}

static int OnEnterSpan(MD_SPANTYPE type, void* detail, void* ud) {
    MdBuild* b = (MdBuild*)ud;
    if (type == MD_SPAN_A) {
        MD_SPAN_A_DETAIL* d = (MD_SPAN_A_DETAIL*)detail;
        b->href = d ? Attr(b->a, &d->href) : Str{};
    }
    // MD_SPAN_IMG has no image loader behind it here; its alt text arrives as
    // ordinary text callbacks and is what ends up on screen.
    b->marks = (uint8_t)(b->marks | SpanMark(type));
    return 0;
}

static int OnLeaveSpan(MD_SPANTYPE type, void* detail, void* ud) {
    (void)detail;
    MdBuild* b = (MdBuild*)ud;
    if (type == MD_SPAN_A) {
        b->href = {};
    }
    b->marks = (uint8_t)(b->marks & ~SpanMark(type));
    return 0;
}

static int OnText(MD_TEXTTYPE type, const MD_CHAR* txt, MD_SIZE size,
                  void* ud) {
    MdBuild* b = (MdBuild*)ud;
    Str s((char*)txt, (int)size);
    switch (type) {
        case MD_TEXT_NULLCHAR:
            AddText(b, StrL("\xEF\xBF\xBD"));
            break;
        case MD_TEXT_BR:
            AddText(b, StrL("\n"));
            break;
        case MD_TEXT_SOFTBR:
            AddText(b, StrL(" "));
            break;
        case MD_TEXT_ENTITY:
            AddText(b, DecodeEntity(b->a, s));
            break;
        case MD_TEXT_HTML:
            // Inline tags are dropped the way Rust drops mdast::Html.
            break;
        default:
            AddText(b, s);
            break;
    }
    return 0;
}

MdNode* MdParse(Arena* a, Str source) {
    MdNode* doc = ArenaNew<MdNode>(a);
    doc->kind = MdKind::Doc;
    if (!source.s || source.len <= 0) {
        return doc;
    }
    MdBuild b;
    b.a = a;
    b.cur = doc;

    MD_PARSER p = {};
    p.abi_version = 0;
    p.flags = MD_DIALECT_GITHUB;
    p.enter_block = OnEnterBlock;
    p.leave_block = OnLeaveBlock;
    p.enter_span = OnEnterSpan;
    p.leave_span = OnLeaveSpan;
    p.text = OnText;
    md_parse(source.s, (MD_SIZE)source.len, &p, &b);
    return doc;
}

// ─── parse cache ──────────────────────────────────────────────────────────
//
// The element tree is rebuilt from the frame arena every frame, but the
// markdown behind it hardly ever changes. Re-parsing the story's 13 KB README
// on every render cost 52us and 44 KB of frame arena for a tree identical to
// the last one, so each window keeps a few parsed documents around, keyed on
// the source text.
//
// A slot owns a copy of the source as well as the nodes, because the tree
// points into the source instead of copying it (see AddText). That way a
// caller may hand us a string that only lives for this frame, and comparing
// the copy is what tells us the cached tree is still the right answer.

struct MdCacheSlot {
    Arena* a = nullptr; // owns `source` and `doc`
    Str source = {};
    MdNode* doc = nullptr;
    uint64_t used = 0; // lookup stamp for LRU; 0 == empty
};

// One story page is on screen at a time and a page holds a handful of these.
constexpr int kMdCacheSlots = 8;

struct MdCache {
    MdCacheSlot slots[kMdCacheSlots] = {};
    uint64_t clock = 0;

    ~MdCache() {
        for (int i = 0; i < kMdCacheSlots; i++) {
            if (slots[i].a) {
                ArenaDelete(slots[i].a);
            }
        }
    }
};

// The whole point is to be much cheaper than a parse. memcmp of the story's
// 13 KB README costs 0.2us against 52us to parse it, so there is no hash to
// reject with first: the length check throws out most misses and memcmp stops
// at the first byte that differs.
static bool MdSourceEq(Str a, Str b) {
    if (a.len != b.len) {
        return false;
    }
    return a.len == 0 || memcmp(a.s, b.s, (size_t)a.len) == 0;
}

// The tree for `source`, parsed only when it isn't cached already. Falls back
// to the frame arena when there is no window to hang a cache off, which is
// what the tests and any headless measuring pass see.
static MdNode* MdParseCached(Ctx* cx, Arena* frame, Str source) {
    MdCache* c = nullptr;
    if (cx && cx->win) {
        auto* slot = KeyedState<Entity<MdCache>>(
            cx, (uint32_t)HashClickId(StrL("gpui-md-parse-cache")));
        if (slot) {
            if (!slot->IsValid()) {
                *slot = EntityNewState<MdCache>(cx->app);
            }
            c = slot->Get(cx);
        }
    }
    if (!c) {
        return MdParse(frame, source);
    }

    c->clock++;
    MdCacheSlot* lru = &c->slots[0];
    for (int i = 0; i < kMdCacheSlots; i++) {
        MdCacheSlot* s = &c->slots[i];
        if (s->used != 0 && MdSourceEq(s->source, source)) {
            s->used = c->clock;
            return s->doc;
        }
        if (s->used < lru->used) {
            lru = s;
        }
    }
    // Evict the least recently looked up slot and reuse its arena.
    if (!lru->a) {
        lru->a = ArenaNew();
    } else {
        lru->a->Reset();
    }
    lru->source = StrDup(lru->a, source);
    lru->doc = MdParse(lru->a, lru->source);
    lru->used = c->clock;
    return lru->doc;
}

// ─── render ───────────────────────────────────────────────────────────────

// node.rs 2258: h1 2.0/BOLD, h2 1.5, h3 1.25, h4 1.125, h5 1.0/SEMIBOLD,
// h6 1.0/MEDIUM.
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

static int HeadingWeight(int level) {
    if (level == 1) {
        return 3;
    }
    if (level >= 6) {
        return 1;
    }
    return 2;
}

static El* ApplyWeight(El* t, int weight) {
    if (weight >= 3) {
        return t->Bold();
    }
    if (weight == 2) {
        return t->Semibold();
    }
    if (weight == 1) {
        return t->Medium();
    }
    return t;
}

// text/utils.rs BULLETS, one per nesting depth.
static Str Bullet(int depth) {
    switch (depth) {
        case 0:
            return StrL("\xE2\x80\xA2 ");
        case 1:
            return StrL("\xE2\x97\xA6 ");
        case 2:
            return StrL("\xE2\x96\xAA ");
        case 3:
            return StrL("\xE2\x80\xA3 ");
        default:
            return StrL("\xE2\x81\x83 ");
    }
}

// text/utils.rs list_item_prefix: 1. at depth 0, A. at depth 1, a. below.
static Str OrderedMarker(Arena* a, int n, int depth) {
    char buf[24];
    if (depth == 0) {
        snprintf(buf, sizeof(buf), "%d. ", n);
        return StrDup(a, Str(buf));
    }
    // `0.` is a legal CommonMark start, so index from 0 rather than n - 1.
    int ix = n > 0 ? (n - 1) % 26 : 0;
    snprintf(buf, sizeof(buf), "%c. ", (depth == 1 ? 'A' : 'a') + ix);
    return StrDup(a, Str(buf));
}

static El* MdWord(Arena* a, const Theme& th, Str w, float font, Rgba color,
                  uint8_t marks, int weight, bool selectable) {
    Rgba c = color;
    if (marks & MdLink) {
        c = th.primary;
    } else if (marks & MdDel) {
        // No strikethrough in the paint layer; muted stands in for it.
        c = th.mutedFg;
    }
    float px = (marks & MdCode) ? font - 1 : font;
    El* t = TextEl(a, w)->Font(px)->Fg(c);
    ApplyWeight(t, (marks & MdBold) ? (weight > 2 ? weight : 2) : weight);
    if (marks & MdItalic) {
        t->Italic();
    }
    if (marks & (MdLink | MdUnderline)) {
        t->Underline();
    }
    if (marks & MdCode) {
        // TextViewStyle::inline_code_highlight falls back to theme.accent.
        t->Mono()->Bg(th.accent);
    }
    if (selectable) {
        t->Selectable();
    }
    return t;
}

static bool IsPlainRun(MdRun* r) {
    if (!r || r->next || r->marks != 0) {
        return false;
    }
    for (int i = 0; i < r->text.len; i++) {
        if (r->text.s[i] == '\n') {
            return false;
        }
    }
    return true;
}

El* TextView::Inline(MdNode* n, float font, Rgba color, int weight) {
    const Theme& th = cx->theme();
    // The common case is one unmarked stretch of source. Keeping it as a
    // single TextEl lets the text engine break the line on its own metrics,
    // which is both better looking and cheaper than a row of word elements.
    if (IsPlainRun(n->runFirst)) {
        El* t = TextEl(a, n->runFirst->text)->Font(font)->Fg(color)->Wrap();
        ApplyWeight(t, weight);
        if (selectable) {
            t->Selectable();
        }
        return t->W(kFill);
    }
    // Otherwise the flow is a column of wrapping rows — a hard break ends a
    // row — and each row is a run of styled words. Every word carries its own
    // trailing space rather than the row carrying a gap: a gap would put a
    // space between an emphasis run and the punctuation after it ("**bold**:"
    // reads as "bold :") and would loosen wrapped-line leading.
    El* col = Div(a)->FlexCol()->W(kFill);
    El* row = Div(a)->FlexRow()->FlexWrap();
    char word[512];
    int len = 0;
    uint8_t marks = 0;
    auto flush = [&]() {
        if (len <= 0) {
            return;
        }
        row->Child(MdWord(a, th, StrDup(a, Str(word, len)), font, color, marks,
                          weight, selectable));
        len = 0;
    };
    for (MdRun* r = n->runFirst; r; r = r->next) {
        flush();
        marks = r->marks;
        for (int i = 0; i < r->text.len; i++) {
            char c = r->text.s[i];
            if (c == '\n') {
                flush();
                col->Child(row);
                row = Div(a)->FlexRow()->FlexWrap();
                continue;
            }
            if (len < (int)sizeof(word) - 1) {
                word[len++] = c;
            }
            if (c == ' ') {
                flush();
            }
        }
    }
    flush();
    col->Child(row);
    return col;
}

static int RunsLen(MdNode* n) {
    int len = 0;
    for (MdRun* r = n->runFirst; r; r = r->next) {
        len += r->text.len;
    }
    return len;
}

El* TextView::CodeBlock(MdNode* n) {
    const Theme& th = cx->theme();
    El* box =
        Div(a)->FlexCol()->W(kFill)->Pad(12)->Radius(th.radius)->Bg(th.muted);
    // The runs are verbatim text with embedded newlines. They go into one
    // TextEl rather than one per line: the text engine then lays every line
    // out against the same metrics, so a line that needs a font fallback (box
    // drawing, CJK) cannot set its own leading and make the block ragged.
    // Nothing wraps, so a long line clips the way a <pre> does.
    int len = RunsLen(n);
    while (len > 0 && n->runLast && n->runLast->text.len > 0 &&
           n->runLast->text.s[n->runLast->text.len - 1] == '\n') {
        // The fence's own trailing newline would paint an empty last line.
        n->runLast->text.len--;
        len--;
    }
    char* buf = (char*)Alloc(a, len + 1);
    if (!buf) {
        return box;
    }
    int at = 0;
    for (MdRun* r = n->runFirst; r; r = r->next) {
        memcpy(buf + at, r->text.s, (size_t)r->text.len);
        at += r->text.len;
    }
    buf[at] = 0;
    El* t = TextEl(a, Str(buf, at))->Font(codeFont)->Fg(th.foreground)->Mono();
    if (selectable) {
        t->Selectable();
    }
    box->Child(t);
    return box;
}

// node.rs render_wrap_table proportions the columns by content length and
// lets them shrink to fit, with a floor per column. Same here: the widths are
// fractions of the table, TableColumnWidth is the floor, and a table whose
// floors do not fit is clipped rather than scrolled — this tree has no
// horizontal scroll area.
//
// Column alignment (`|:--:|`) is parsed but not applied; the paint layer has
// no text-align.
El* TextView::Table(MdNode* n) {
    enum {
        kMaxCols = 32,
        // node.rs MAX_LENGTH: one long cell must not starve the rest.
        kMaxLen = 150
    };
    const Theme& th = cx->theme();
    int colLen[kMaxCols] = {};
    int nCols = 0;
    for (MdNode* r = n->first; r; r = r->next) {
        int ix = 0;
        for (MdNode* c = r->first; c && ix < kMaxCols; c = c->next, ix++) {
            int len = RunsLen(c);
            if (len > kMaxLen) {
                len = kMaxLen;
            }
            if (len > colLen[ix]) {
                colLen[ix] = len;
            }
            if (ix + 1 > nCols) {
                nCols = ix + 1;
            }
        }
    }
    float total = 0;
    for (int i = 0; i < nCols; i++) {
        // An empty column still needs room for its border and padding.
        if (colLen[i] < 4) {
            colLen[i] = 4;
        }
        total += (float)colLen[i];
    }
    if (total <= 0) {
        return Div(a);
    }

    El* table =
        Div(a)->FlexCol()->W(kFill)->Border(1, th.border)->Radius(th.radius);
    for (MdNode* r = n->first; r; r = r->next) {
        El* row = Div(a)->FlexRow()->W(kFill);
        if (r->next) {
            row->BorderB(1, th.border);
        }
        int ix = 0;
        for (MdNode* c = r->first; c; c = c->next, ix++) {
            float frac = ix < nCols ? (float)colLen[ix] / total : 1.f / total;
            El* cell = Div(a)->WFrac(frac)->MinW(tableColW)->PadX(8)->PadY(4);
            if (c->next) {
                cell->BorderR(1, th.border);
            }
            cell->Child(Inline(c, baseFont, th.foreground, r->head ? 2 : 0));
            row->Child(cell);
        }
        table->Child(row);
    }
    return table;
}

El* TextView::Item(MdNode* n, Str marker, int depth) {
    const Theme& th = cx->theme();
    El* content = Div(a)->FlexCol()->Grow();
    // md4c omits MD_BLOCK_P inside a tight list (md4c.c 4842), so an item's
    // first paragraph arrives as runs on the item itself.
    if (n->runFirst) {
        content->Child(Inline(n, baseFont, th.foreground, 0));
    }
    Blocks(content, n, depth, true);
    return Div(a)
        ->FlexRow()
        ->W(kFill)
        ->ItemsStart()
        ->Child(TextEl(a, marker)->Font(baseFont)->Fg(th.mutedFg)->Shrink0())
        ->Child(content);
}

El* TextView::Blocks(El* into, MdNode* n, int depth, bool inList) {
    for (MdNode* c = n->first; c; c = c->next) {
        El* e = Block(c, depth, inList, c->next == nullptr);
        if (e) {
            into->Child(e);
        }
    }
    return into;
}

El* TextView::Block(MdNode* n, int depth, bool inList, bool isLast) {
    const Theme& th = cx->theme();
    // node.rs render_block: every block but the last one in its container
    // carries the paragraph gap below it, and a block inside a list item
    // carries none.
    float mb = (inList || isLast) ? 0.f : paragraphGap;
    switch (n->kind) {
        case MdKind::Paragraph:
            return Div(a)->W(kFill)->PadB(mb)->Child(
                Inline(n, baseFont, th.foreground, 0));
        case MdKind::Heading: {
            float font = headingFont * HeadingScale(n->level);
            // Headings use their own 0.3rem bottom padding, not the gap.
            return Div(a)->W(kFill)->PadB(5)->Child(
                Inline(n, font, th.foreground, HeadingWeight(n->level)));
        }
        case MdKind::Rule:
            return Div(a)->W(kFill)->PadB(mb)->Child(
                Div(a)->H(2)->W(kFill)->Bg(th.border));
        case MdKind::Quote: {
            El* inner = Div(a)
                            ->FlexCol()
                            ->W(kFill)
                            ->Fg(th.mutedFg)
                            ->BorderL(3, th.secondaryActive)
                            ->PadX(16);
            Blocks(inner, n, depth, false);
            return Div(a)->W(kFill)->PadB(mb)->Child(inner);
        }
        case MdKind::List: {
            El* list = Div(a)->FlexCol()->W(kFill)->MinW(0)->PadB(mb);
            int ix = n->start;
            for (MdNode* c = n->first; c; c = c->next) {
                if (c->kind != MdKind::Item) {
                    continue;
                }
                Str marker =
                    n->ordered ? OrderedMarker(a, ix, depth) : Bullet(depth);
                list->Child(Item(c, marker, depth + 1));
                ix++;
            }
            return list;
        }
        case MdKind::Code:
            return Div(a)->W(kFill)->PadB(mb)->Child(CodeBlock(n));
        case MdKind::Table:
            return Div(a)->W(kFill)->PadB(mb)->Child(Table(n));
        case MdKind::Item: {
            // Only reached for a stray list item; treat it as its contents.
            El* box = Div(a)->FlexCol()->W(kFill)->PadB(mb);
            if (n->runFirst) {
                box->Child(Inline(n, baseFont, th.foreground, 0));
            }
            return Blocks(box, n, depth, inList);
        }
        case MdKind::Html:
        case MdKind::Doc:
        case MdKind::Row:
        case MdKind::Cell:
            return nullptr;
    }
    return nullptr;
}

El* TextView::IntoEl() {
    MdNode* doc = MdParseCached(cx, a, source);
    return Blocks(Div(a)->FlexCol()->W(kFill), doc, 0, false);
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

TextView* TextView::HeadingFont(float px) {
    headingFont = px;
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

TextView* TextView::ParagraphGap(float px) {
    paragraphGap = px;
    return this;
}

} // namespace component
} // namespace gpui
