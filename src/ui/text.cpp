#include "ui/text.h"

#include "gpui/image.h"
#include "gpui/paint.h"
#include "markdown/markdown.h"
#include "ui/html.h"

namespace gpui {

namespace component {

// ─── parse ────────────────────────────────────────────────────────────────
//
// src/markdown is the `markdown` crate, ported: it hands back an mdast, the
// same tree Rust gets from `markdown::to_mdast`. This walk folds that tree
// into the MdNode tree below, which is what crates/ui/src/text/format/
// markdown.rs does with `ast_to_node` and `parse_paragraph`.

namespace md = markdown;

// A link reference definition, kept so `[text][id]` can find its URL.
// crates/ui/src/text/format/markdown.rs puts these in the NodeContext with
// `cx.add_ref`; there is one parse here, so they live with the builder.
struct MdDef {
    Str identifier;
    Str url;
};

struct MdBuild {
    Arena* a = nullptr;
    MdNode* cur = nullptr;
    // The marks in effect, from the enclosing inline nodes.
    uint8_t marks = 0;
    Str href = {};
    ArenaVec<MdDef> defs = {};
};

// A node's strings are ArenaStr — an offset into the arena the tree was
// parsed into, which is the builder's own. This reads one back.
// One of a node's strings, or an empty Str when it does not carry that one.
static Str V(MdBuild* b, const md::Node* n, md::NodeStrKind k) {
    return md::NodeGetStr(b->a, n, k);
}

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
// pointing straight at the tree's text with nothing copied.
static void AddText(MdBuild* b, Str s) {
    if (s.len <= 0) {
        return;
    }
    MdNode* n = b->cur;
    MdRun* r = n->runLast;
    if (r && !r->imgSrc.s && r->marks == b->marks && r->href.s == b->href.s &&
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

// node.rs InlineNode::image: an image sits in the flow beside the words,
// carrying the marks in force — an image inside a link is a link.
static void AddImage(MdBuild* b, Str src, Str alt, float w, float h) {
    if (src.len <= 0) {
        return;
    }
    MdNode* n = b->cur;
    MdRun* r = ArenaNew<MdRun>(b->a);
    r->imgSrc = src;
    r->text = alt;
    r->imgW = w;
    r->imgH = h;
    r->marks = b->marks;
    r->href = b->href;
    if (n->runLast) {
        n->runLast->next = r;
    } else {
        n->runFirst = r;
    }
    n->runLast = r;
}

// "&amp;" -> "&". Returns the entity unchanged when it is not one the crate's
// table knows. ui/html.cpp decodes the entities in an attribute and in HTML
// text with it; the markdown side needs no such thing, because the parser
// decodes character references itself.
Str MdDecodeEntity(Arena* a, Str e) {
    if (e.len < 3 || e.s[0] != '&' || e.s[e.len - 1] != ';') {
        return e;
    }
    Str body((char*)e.s + 1, e.len - 2);
    Str value;
    if (body.len > 1 && body.s[0] == '#') {
        if (body.s[1] == 'x' || body.s[1] == 'X') {
            value =
                md::DecodeNumeric(a, Str((char*)body.s + 2, body.len - 2), 16);
        } else {
            value =
                md::DecodeNumeric(a, Str((char*)body.s + 1, body.len - 1), 10);
        }
    } else {
        value = md::DecodeNamed(a, body);
    }
    return value.s ? value : e;
}

// One inline tag inside a paragraph. The markdown parser hands `<b>` over as
// an mdast Html node and leaves the meaning to us; Rust reaches the same tags
// through html5ever, since markdown.rs sends the node to format::html. A raw
// HTML *block* is a node of its own and is parsed whole, below.
static void MdInlineHtml(MdBuild* b, Str tag) {
    if (b->cur->kind == MdKind::Html) {
        // Inside a raw HTML block every byte is source, tags included.
        AddText(b, tag);
        return;
    }
    HtmlInlineTag t = HtmlParseInlineTag(b->a, tag);
    if (!t.known) {
        // An unknown tag is dropped, the way Rust drops what its own
        // vocabulary does not cover.
        return;
    }
    if (t.isBreak) {
        AddText(b, StrL("\n"));
        return;
    }
    if (t.isImage) {
        AddImage(b, t.src, t.alt, t.width, t.height);
        return;
    }
    if (t.close) {
        b->marks = (uint8_t)(b->marks & ~t.mark);
        if (t.mark & MdLink) {
            b->href = {};
        }
        return;
    }
    b->marks = (uint8_t)(b->marks | t.mark);
    if (t.mark & MdLink) {
        b->href = t.href;
    }
}

// ─── the mdast walk ───────────────────────────────────────────────────────

static void MdInlineNode(MdBuild* b, const md::Node* n);

static void MdInlineChildren(MdBuild* b, const md::Node* n) {
    for (const md::Node* child : md::NodeKids(b->a, n)) {
        MdInlineNode(b, child);
    }
}

// The children of `n` with `mark` added to whatever is already in force,
// which is markdown.rs's merge_children_with_mark.
static void MdMarked(MdBuild* b, const md::Node* n, uint8_t mark) {
    uint8_t saved = b->marks;
    b->marks = (uint8_t)(b->marks | mark);
    MdInlineChildren(b, n);
    b->marks = saved;
}

// The URL a `[text][id]` or `![alt][id]` points at, from the definitions
// collected below. Empty when the definition is missing, which is what Rust's
// LinkMark holds until the reference is resolved.
static Str MdDefUrl(MdBuild* b, Str identifier) {
    for (const MdDef& def : b->defs) {
        if (def.identifier.len == identifier.len &&
            memcmp(def.identifier.s, identifier.s, (size_t)identifier.len) ==
                0) {
            return def.url;
        }
    }
    return {};
}

static void MdInlineNode(MdBuild* b, const md::Node* n) {
    switch (n->kind) {
        case md::NodeKind::Text:
            AddText(b, V(b, n, md::NodeStrKind::Value));
            break;
        case md::NodeKind::Emphasis:
            MdMarked(b, n, MdItalic);
            break;
        case md::NodeKind::Strong:
            MdMarked(b, n, MdBold);
            break;
        case md::NodeKind::Delete:
            MdMarked(b, n, MdDel);
            break;
        case md::NodeKind::InlineCode:
        case md::NodeKind::InlineMath: {
            uint8_t saved = b->marks;
            b->marks = (uint8_t)(b->marks | MdCode);
            AddText(b, V(b, n, md::NodeStrKind::Value));
            b->marks = saved;
            break;
        }
        case md::NodeKind::Break:
            // Rust drops an inline break (parse_paragraph has no arm for it);
            // a hard break is what starts a new row of the flow here, so it
            // stays.
            AddText(b, StrL("\n"));
            break;
        case md::NodeKind::Link:
        case md::NodeKind::LinkReference: {
            Str saved = b->href;
            b->href = n->kind == md::NodeKind::Link
                          ? V(b, n, md::NodeStrKind::Url)
                          : MdDefUrl(b, V(b, n, md::NodeStrKind::Identifier));
            MdMarked(b, n, MdLink);
            b->href = saved;
            break;
        }
        case md::NodeKind::Image:
            AddImage(b, V(b, n, md::NodeStrKind::Url),
                     V(b, n, md::NodeStrKind::Alt), 0, 0);
            break;
        case md::NodeKind::ImageReference:
            AddImage(b, MdDefUrl(b, V(b, n, md::NodeStrKind::Identifier)),
                     V(b, n, md::NodeStrKind::Alt), 0, 0);
            break;
        case md::NodeKind::FootnoteReference: {
            // markdown.rs renders the call as an italic `[id]`.
            uint8_t saved = b->marks;
            b->marks = (uint8_t)(b->marks | MdItalic);
            AddText(b, StrL("["));
            AddText(b, V(b, n, md::NodeStrKind::Identifier));
            AddText(b, StrL("]"));
            b->marks = saved;
            break;
        }
        case md::NodeKind::Html:
            MdInlineHtml(b, V(b, n, md::NodeStrKind::Value));
            break;
        default:
            // Anything else is not inline content; Rust warns and drops it.
            break;
    }
}

// The inline children of `n` as the runs of the current block.
static void MdInline(MdBuild* b, const md::Node* n) {
    MdInlineChildren(b, n);
}

static void MdBlockNode(MdBuild* b, const md::Node* n);

static void MdBlockChildren(MdBuild* b, const md::Node* n) {
    for (const md::Node* child : md::NodeKids(b->a, n)) {
        MdBlockNode(b, child);
    }
}

// A code block, however the source spelled it: a fence, an indent, math, or
// the document's frontmatter.
static void MdCodeBlock(MdBuild* b, Str value, Str lang) {
    MdNode* n = Push(b, MdKind::Code);
    n->lang = lang;
    AddText(b, value);
    Pop(b);
}

static void MdTable(MdBuild* b, const md::Node* n) {
    MdNode* table = Push(b, MdKind::Table);
    (void)table;
    int32_t rowIndex = 0;
    for (const md::Node* row : md::NodeKids(b->a, n)) {
        int32_t at = rowIndex++;
        if (row->kind != md::NodeKind::TableRow) {
            continue;
        }
        MdNode* r = Push(b, MdKind::Row);
        // mdast has no thead: the first row is the head.
        r->head = at == 0;
        int32_t cellIndex = 0;
        for (const md::Node* cell : md::NodeKids(b->a, row)) {
            if (cell->kind != md::NodeKind::TableCell) {
                continue;
            }
            int32_t column = cellIndex++;
            MdNode* c = Push(b, MdKind::Cell);
            md::ArenaAlign align = md::NodePerKind(b->a, n);
            if (column < md::ArenaAlignCount(b->a, align)) {
                switch (md::ArenaAlignAt(b->a, align, column)) {
                    case md::AlignKind::Left:
                        c->align = MdAlignLeft;
                        break;
                    case md::AlignKind::Center:
                        c->align = MdAlignCenter;
                        break;
                    case md::AlignKind::Right:
                        c->align = MdAlignRight;
                        break;
                    case md::AlignKind::None:
                        c->align = MdAlignDefault;
                        break;
                }
            }
            MdInline(b, cell);
            Pop(b);
        }
        Pop(b);
    }
    Pop(b);
}

static void MdBlockNode(MdBuild* b, const md::Node* n) {
    switch (n->kind) {
        case md::NodeKind::Paragraph:
            Push(b, MdKind::Paragraph);
            MdInline(b, n);
            Pop(b);
            break;
        case md::NodeKind::Heading: {
            MdNode* h = Push(b, MdKind::Heading);
            uint32_t depth = md::NodePerKind(b->a, n);
            h->level = depth == 0 ? 1 : (uint8_t)depth;
            MdInline(b, n);
            Pop(b);
            break;
        }
        case md::NodeKind::Blockquote:
            Push(b, MdKind::Quote);
            MdBlockChildren(b, n);
            Pop(b);
            break;
        case md::NodeKind::List: {
            MdNode* l = Push(b, MdKind::List);
            l->ordered = n->Has(md::NodeOrdered);
            l->start =
                n->Has(md::NodeHasStart) ? (int)md::NodePerKind(b->a, n) : 1;
            MdBlockChildren(b, n);
            Pop(b);
            break;
        }
        case md::NodeKind::ListItem:
            Push(b, MdKind::Item);
            MdBlockChildren(b, n);
            Pop(b);
            break;
        case md::NodeKind::ThematicBreak:
            Push(b, MdKind::Rule);
            Pop(b);
            break;
        case md::NodeKind::Code:
            MdCodeBlock(b, V(b, n, md::NodeStrKind::Value),
                        V(b, n, md::NodeStrKind::Lang));
            break;
        case md::NodeKind::Math:
            MdCodeBlock(b, V(b, n, md::NodeStrKind::Value), {});
            break;
        case md::NodeKind::Yaml:
            MdCodeBlock(b, V(b, n, md::NodeStrKind::Value), StrL("yml"));
            break;
        case md::NodeKind::Toml:
            MdCodeBlock(b, V(b, n, md::NodeStrKind::Value), StrL("toml"));
            break;
        case md::NodeKind::Table:
            MdTable(b, n);
            break;
        case md::NodeKind::Html:
            // The raw source of the block; MdExpandHtml below turns it into
            // children.
            Push(b, MdKind::Html);
            AddText(b, V(b, n, md::NodeStrKind::Value));
            Pop(b);
            break;
        case md::NodeKind::Break:
            Push(b, MdKind::Paragraph);
            AddText(b, StrL("\n"));
            Pop(b);
            break;
        case md::NodeKind::FootnoteDefinition: {
            // markdown.rs renders the definition as a paragraph opening with
            // an italic `[id]: `.
            Push(b, MdKind::Paragraph);
            uint8_t saved = b->marks;
            b->marks = (uint8_t)(b->marks | MdItalic);
            AddText(b, StrL("["));
            AddText(b, V(b, n, md::NodeStrKind::Identifier));
            AddText(b, StrL("]: "));
            b->marks = saved;
            for (const md::Node* c : md::NodeKids(b->a, n)) {
                // Its children are blocks; their inline content joins the one
                // paragraph, which is what Rust's parse_paragraph does.
                if (md::NodeHasChildren(c->kind)) {
                    MdInline(b, c);
                } else {
                    AddText(b, V(b, c, md::NodeStrKind::Value));
                }
            }
            Pop(b);
            break;
        }
        case md::NodeKind::Definition:
            // Collected before the walk; it renders as nothing.
            break;
        default:
            break;
    }
}

// Every link reference definition in the tree, wherever it sits.
static void MdCollectDefs(MdBuild* b, const md::Node* n) {
    if (n->kind == md::NodeKind::Definition) {
        MdDef def;
        def.identifier = V(b, n, md::NodeStrKind::Identifier);
        def.url = V(b, n, md::NodeStrKind::Url);
        b->defs.Append(b->a, def);
        return;
    }
    for (const md::Node* child : md::NodeKids(b->a, n)) {
        MdCollectDefs(b, child);
    }
}

// A raw HTML block arrives as source text on an MdKind::Html node. Turning
// it into children here rather than at render time means the parse cache
// holds the finished tree, and it is the same hand-off Rust makes when
// markdown.rs gives an mdast::Html node to format::html.
static void MdExpandHtml(Arena* a, MdNode* n) {
    for (MdNode* c = n->first; c; c = c->next) {
        MdExpandHtml(a, c);
    }
    if (n->kind != MdKind::Html || !n->runFirst) {
        return;
    }
    int len = 0;
    for (MdRun* r = n->runFirst; r; r = r->next) {
        len += r->text.len;
    }
    char* buf = (char*)Alloc(a, len + 1);
    if (!buf) {
        return;
    }
    int at = 0;
    for (MdRun* r = n->runFirst; r; r = r->next) {
        memcpy(buf + at, r->text.s, (size_t)r->text.len);
        at += r->text.len;
    }
    buf[at] = 0;
    n->runFirst = nullptr;
    n->runLast = nullptr;
    HtmlParseInto(a, n, Str(buf, at));
}

MdNode* MdParse(Arena* a, Str source) {
    MdNode* doc = ArenaNew<MdNode>(a);
    doc->kind = MdKind::Doc;
    if (!source.s || source.len <= 0) {
        return doc;
    }

    // The GFM dialect, which is what TextView renders: tables, strikethrough,
    // task lists, footnotes and bare-URL autolinks. `parse_options` in
    // crates/ui/src/text/markdown_ext.rs asks for the same.
    md::Node* root = md::ToMdast(a, source, md::ParseOptions::Gfm());

    MdBuild b;
    b.a = a;
    b.cur = doc;
    MdCollectDefs(&b, root);
    MdBlockChildren(&b, root);
    MdExpandHtml(a, doc);
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
    // Which parser made `doc`. The same bytes are a different tree read as
    // HTML than read as markdown, so it is part of the key.
    bool html = false;
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
static MdNode* MdParseCached(Ctx* cx, Arena* frame, Str source, bool html) {
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
        return html ? HtmlParse(frame, source) : MdParse(frame, source);
    }

    c->clock++;
    MdCacheSlot* lru = &c->slots[0];
    for (int i = 0; i < kMdCacheSlots; i++) {
        MdCacheSlot* s = &c->slots[i];
        if (s->used != 0 && s->html == html && MdSourceEq(s->source, source)) {
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
    lru->html = html;
    lru->doc =
        html ? HtmlParse(lru->a, lru->source) : MdParse(lru->a, lru->source);
    lru->used = c->clock;
    return lru->doc;
}

// ─── render ───────────────────────────────────────────────────────────────

// The fallback handle_link_click takes when no handler was given: cx.open_url.
static void MdOpenHref(char* href) {
    if (href) {
        OpenUrl(Str(href));
    }
}

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

// `<mark>`: html.rs takes yellow(200) — a fixed Tailwind step, not a theme
// color — and leaves the ink alone, which works because the foreground it
// runs against there is dark. Ours is near-white in the dark theme, so the
// ink is pinned to the same near-black the light theme already paints with
// and the highlight reads the same in both.
static const Rgba kMarkBg = {0xfe, 0xf0, 0x8a, 0xff};
static const Rgba kMarkFg = {0x0a, 0x0a, 0x0a, 0xff};

// node.rs puts an img() element in the flow beside the words: its own size
// unless the document gave one, never wider than the space it has, and — for
// an image inside a link — the hand and the click the link's words get.
El* TextView::ImageRun(MdRun* r, float font, Rgba color) {
    El* e = ImageEl(a, r->imgSrc, r->text)->Font(font)->Fg(color);
    float w = r->imgW;
    float h = r->imgH;
    // A vector picture knows its own shape, so a document that gave only one
    // dimension gets the other rather than a run of text's line height. A
    // bitmap cannot answer that without being decoded, so this is the SVG
    // case only — the shipped asset, or one fetched, once it has arrived.
    if ((w > 0) != (h > 0)) {
        Size vb = {};
        int opsLen = 0;
        const uint8_t* ops = ImageVectorForSrc(r->imgSrc, &opsLen);
        if (ops && DrawOpsViewBox(ops, opsLen, &vb) && vb.w > 0 && vb.h > 0) {
            if (w > 0) {
                h = w * (vb.h / vb.w);
            } else {
                w = h * (vb.w / vb.h);
            }
        }
    }
    if (w > 0) {
        e->W(w);
    }
    if (h > 0) {
        e->H(h);
    }
    if ((r->marks & MdLink) && r->href.len > 0) {
        e->Cursor(CursorKind::Pointer);
        if (onLink.IsValid()) {
            e->OnClick(ListenerArg(onLink, (intptr_t)r->href.s));
        } else {
            e->OnClick(MkFunc0(MdOpenHref, r->href.s));
        }
    }
    return e;
}

// One styled word. Everything a TextMark can say about a run, applied to the
// element that carries it — node.rs 1390 builds the same HighlightStyle.
El* TextView::Word(Str w, float font, Rgba color, uint8_t marks, int weight,
                   Str href) {
    const Theme& th = cx->theme();
    Rgba c = color;
    if (marks & MdLink) {
        c = th.primary;
    }
    if (marks & MdHighlight) {
        c = kMarkFg;
    }
    El* t = TextEl(a, w)->Font(font)->Fg(c);
    ApplyWeight(t, (marks & MdBold) ? (weight > 2 ? weight : 2) : weight);
    if (marks & MdItalic) {
        t->Italic();
    }
    if (marks & (MdLink | MdUnderline)) {
        t->Underline();
    }
    if (marks & MdDel) {
        t->Strikethrough();
    }
    if (marks & MdHighlight) {
        t->Bg(kMarkBg);
    } else if (marks & MdCode) {
        // TextViewStyle::inline_code_highlight falls back to theme.accent,
        // and says nothing else: an inline code span is the paragraph's own
        // font at the paragraph's own size, with a background behind it.
        t->Bg(th.tokens.accent);
    }
    if (selectable) {
        t->Selectable();
    }
    if ((marks & MdLink) && href.len > 0) {
        // handle_link_click: the handler if one was given, the desktop's
        // browser otherwise. The href is NUL-terminated in the arena the
        // parse lives in, so the handler gets a `const char*` it can read for
        // the length of the call — the same rule every hit-test payload
        // follows.
        t->Cursor(CursorKind::Pointer);
        if (onLink.IsValid()) {
            t->OnClick(ListenerArg(onLink, (intptr_t)href.s));
        } else {
            t->OnClick(MkFunc0(MdOpenHref, href.s));
        }
    }
    return t;
}

static bool IsPlainRun(MdRun* r) {
    if (!r || r->next || r->marks != 0 || r->imgSrc.len > 0) {
        return false;
    }
    for (int i = 0; i < r->text.len; i++) {
        if (r->text.s[i] == '\n') {
            return false;
        }
    }
    return true;
}

// A cell's text-align. Rust gives the flow itself the alignment (node.rs
// render_wrap_table); here the row of words is a flex line, so the line is
// what justifies. A left-aligned or default flow still fills the cell, which
// is what lets a long one wrap.
static El* AlignRow(El* row, uint8_t align) {
    if (align == MdAlignCenter) {
        return row->JustifyCenter();
    }
    if (align == MdAlignRight) {
        return row->JustifyEnd();
    }
    return row;
}

El* TextView::Inline(MdNode* n, float font, Rgba color, int weight,
                     uint8_t align) {
    // The common case is one unmarked stretch of source. Keeping it as a
    // single TextEl lets the text engine break the line on its own metrics,
    // which is both better looking and cheaper than a row of word elements.
    if (IsPlainRun(n->runFirst)) {
        El* t = TextEl(a, n->runFirst->text)->Font(font)->Fg(color)->Wrap();
        ApplyWeight(t, weight);
        if (selectable) {
            t->Selectable();
        }
        if (align == MdAlignCenter || align == MdAlignRight) {
            // The text shrink-wraps so the box around it can push it over.
            return AlignRow(Div(a)->FlexRow()->W(kFill), align)->Child(t);
        }
        return t->W(kFill);
    }
    // Otherwise the flow is a column of wrapping rows — a hard break ends a
    // row — and each row is a run of styled words. Every word carries its own
    // trailing space rather than the row carrying a gap: a gap would put a
    // space between an emphasis run and the punctuation after it ("**bold**:"
    // reads as "bold :") and would loosen wrapped-line leading.
    El* col = Div(a)->FlexCol()->W(kFill);
    El* row = AlignRow(Div(a)->FlexRow()->FlexWrap()->W(kFill), align);
    char word[512];
    int len = 0;
    uint8_t marks = 0;
    Str href = {};
    auto flush = [&]() {
        if (len <= 0) {
            return;
        }
        row->Child(
            Word(StrDup(a, Str(word, len)), font, color, marks, weight, href));
        len = 0;
    };
    for (MdRun* r = n->runFirst; r; r = r->next) {
        flush();
        marks = r->marks;
        href = r->href;
        if (r->imgSrc.len > 0) {
            row->Child(ImageRun(r, font, color));
            continue;
        }
        for (int i = 0; i < r->text.len; i++) {
            char c = r->text.s[i];
            if (c == '\n') {
                flush();
                col->Child(row);
                row = AlignRow(Div(a)->FlexRow()->FlexWrap()->W(kFill), align);
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
    El* box = Div(a)->FlexCol()->W(kFill)->Pad(12)->Radius(th.radius)->Bg(
        th.tokens.muted);
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
    SyntaxLang lang = SyntaxLangFor(n->lang);
    if (lang != SyntaxLangNone) {
        box->Child(CodeLines(Str(buf, at), lang));
        return box;
    }
    El* t = TextEl(a, Str(buf, at))->Font(codeFont)->Fg(th.foreground)->Mono();
    if (selectable) {
        t->Selectable();
    }
    box->Child(t);
    return box;
}

// The highlighted form: one row per line, one element per run of a color.
// This is where the single-TextEl argument above gives way — a line has to
// be several elements to be several colors — so the rows carry the line box
// themselves and every element in them is the same mono face at the same
// size, which keeps the lines from setting their own leading.
El* TextView::CodeLines(Str code, SyntaxLang lang) {
    const Theme& th = cx->theme();
    ThemeMode mode = cx->themeMode();
    El* col = Div(a)->FlexCol()->W(kFill);
    float lineH = codeFont * kLineHeight;
    El* row = Div(a)->FlexRow()->H(lineH);
    // The run being gathered: adjacent tokens of one color are one element,
    // which keeps a line of code down to a handful.
    char piece[512];
    int len = 0;
    Rgba color = th.foreground;
    auto flush = [&]() {
        if (len <= 0) {
            return;
        }
        El* t = TextEl(a, StrDup(a, Str(piece, len)))
                    ->Font(codeFont)
                    ->Fg(color)
                    ->Mono();
        if (selectable) {
            t->Selectable();
        }
        row->Child(t);
        len = 0;
    };

    SyntaxLexer lx;
    SyntaxLexStart(&lx, lang, code);
    while (SyntaxLexNext(&lx)) {
        Rgba c = SyntaxTokColor(lx.tok, mode, th.foreground);
        for (int i = 0; i < lx.text.len; i++) {
            char ch = lx.text.s[i];
            if (ch == '\n') {
                flush();
                col->Child(row);
                row = Div(a)->FlexRow()->H(lineH);
                continue;
            }
            if (ch == '\r') {
                continue;
            }
            if (c.a != color.a || c.r != color.r || c.g != color.g ||
                c.b != color.b) {
                flush();
                color = c;
            }
            if (len < (int)sizeof(piece) - 1) {
                piece[len++] = ch;
            }
        }
    }
    flush();
    col->Child(row);
    return col;
}

// node.rs render_wrap_table proportions the columns by content length and
// lets them shrink to fit, with a floor per column. Same here: the widths are
// fractions of the table, TableColumnWidth is the floor, and a table whose
// floors do not fit is clipped rather than scrolled — this tree has no
// horizontal scroll area.
//
// Column alignment is the delimiter row's (`|:--:|`) or, for an HTML table,
// the cell's align attribute. A body cell with none of its own takes the
// header cell's, which is how a markdown table says it once.
El* TextView::Table(MdNode* n) {
    enum {
        kMaxCols = 32,
        // node.rs MAX_LENGTH: one long cell must not starve the rest.
        kMaxLen = 150
    };
    const Theme& th = cx->theme();
    int colLen[kMaxCols] = {};
    uint8_t colAlign[kMaxCols] = {};
    int nCols = 0;
    for (MdNode* r = n->first; r; r = r->next) {
        int ix = 0;
        for (MdNode* c = r->first; c && ix < kMaxCols; c = c->next, ix++) {
            if (colAlign[ix] == MdAlignDefault) {
                colAlign[ix] = c->align;
            }
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
            uint8_t align = c->align;
            if (align == MdAlignDefault && ix < nCols) {
                align = colAlign[ix];
            }
            cell->Child(Inline(c, baseFont, BlockFg(), r->head ? 2 : 0, align));
            row->Child(cell);
        }
        table->Child(row);
    }
    return table;
}

Rgba TextView::BlockFg() const {
    return blockFgSet ? blockFg : cx->theme().foreground;
}

El* TextView::Item(MdNode* n, Str marker, int depth) {
    El* content = Div(a)->FlexCol()->Flex1()->MinW(0)->ClipX();
    // An item's blocks are below; runs sit on the item itself only when
    // something built the tree by hand, since mdast gives even a tight list
    // item a paragraph of its own.
    if (n->runFirst) {
        content->Child(Inline(n, baseFont, BlockFg(), 0));
    }
    Blocks(content, n, depth, true);
    return Div(a)
        ->FlexRow()
        ->W(kFill)
        ->ItemsStart()
        // list_item_prefix is a plain string child: it takes the color the
        // list inherits, so a bullet inside a red alert is red.
        ->Child(TextEl(a, marker)->Font(baseFont)->Shrink0())
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
                Inline(n, baseFont, BlockFg(), 0));
        case MdKind::Heading: {
            float font = headingFont * HeadingScale(n->level);
            // Headings use their own 0.3rem bottom padding, not the gap.
            return Div(a)->W(kFill)->PadB(5)->Child(
                Inline(n, font, BlockFg(), HeadingWeight(n->level)));
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
            // text_color(muted_foreground) on the quote, and nothing inside
            // it naming a colour of its own: the paragraphs and headings it
            // holds inherit the grey rather than painting themselves black.
            Rgba savedFg = blockFg;
            bool savedSet = blockFgSet;
            blockFg = th.mutedFg;
            blockFgSet = true;
            Blocks(inner, n, depth, false);
            blockFg = savedFg;
            blockFgSet = savedSet;
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
                box->Child(Inline(n, baseFont, BlockFg(), 0));
            }
            return Blocks(box, n, depth, inList);
        }
        case MdKind::Html:
        case MdKind::Group: {
            // BlockNode::Root: the children in the parent's flow, no box.
            // The gap goes on the group rather than on its last child, so a
            // <div> of paragraphs sits the same distance from what follows it
            // as a paragraph would.
            if (!n->first) {
                return nullptr;
            }
            El* box = Div(a)->FlexCol()->W(kFill)->PadB(mb);
            return Blocks(box, n, depth, inList);
        }
        case MdKind::Doc:
        case MdKind::Row:
        case MdKind::Cell:
            return nullptr;
    }
    return nullptr;
}

El* TextView::IntoEl() {
    MdNode* doc = MdParseCached(cx, a, source, html);
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

TextView* TextView::NewHtml(Ctx* cx, Str source) {
    TextView* t = TextView::New(cx, source);
    t->html = true;
    return t;
}

TextView* TextView::OnLink(Listener fn) {
    onLink = fn;
    return this;
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
