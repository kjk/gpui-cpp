#include "ui/html.h"

namespace gpui {

namespace component {

// ─── lexer ────────────────────────────────────────────────────────────────
//
// html5ever runs a whole HTML5 tokenizer; this is the part of it a document
// written for a reader — Rust's own bar, "simple HTML like Safari Reader
// Mode" — actually uses: text, start tags with attributes, end tags, and the
// two things to throw away, comments and the doctype.

enum class HtmlTok : uint8_t {
    End,
    Text,
    Start,
    Close
};

static bool HtmlIsSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

static bool HtmlIsAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool HtmlIsNameChar(char c) {
    return HtmlIsAlpha(c) || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
           c == ':';
}

static char HtmlLower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

struct HtmlLex {
    Str src = {};
    int at = 0;
    HtmlTok tok = HtmlTok::End;
    // Text: the raw source slice, entities and all.
    Str text = {};
    // Start / Close: the tag name, lowercased, pointing at `nameBuf`.
    Str name = {};
    // Start: everything between the name and the '>'.
    Str attrs = {};
    bool selfClose = false;
    // A tag name longer than this is not one we know; truncating it only
    // means it lands in the same "unknown tag" arm.
    char nameBuf[32] = {};
};

static void HtmlLexName(HtmlLex* l) {
    int n = 0;
    while (l->at < l->src.len && HtmlIsNameChar(l->src.s[l->at])) {
        char c = HtmlLower(l->src.s[l->at++]);
        if (n < (int)sizeof(l->nameBuf) - 1) {
            l->nameBuf[n++] = c;
        }
    }
    l->nameBuf[n] = 0;
    l->name = Str(l->nameBuf, n);
}

// From the name to the '>', with quoted values passed over so that a '>'
// inside one does not end the tag.
static void HtmlLexAttrs(HtmlLex* l) {
    int start = l->at;
    char quote = 0;
    while (l->at < l->src.len) {
        char c = l->src.s[l->at];
        if (quote) {
            if (c == quote) {
                quote = 0;
            }
        } else if (c == '"' || c == '\'') {
            quote = c;
        } else if (c == '>') {
            break;
        }
        l->at++;
    }
    int end = l->at;
    if (l->at < l->src.len) {
        l->at++; // the '>'
    }
    while (end > start && HtmlIsSpace(l->src.s[end - 1])) {
        end--;
    }
    l->selfClose = end > start && l->src.s[end - 1] == '/';
    if (l->selfClose) {
        end--;
    }
    l->attrs = Str(l->src.s + start, end - start);
}

static bool HtmlAtStartTag(const HtmlLex* l, int at) {
    if (at + 1 >= l->src.len || l->src.s[at] != '<') {
        return false;
    }
    char c = l->src.s[at + 1];
    return HtmlIsAlpha(c) || c == '/' || c == '!';
}

static void HtmlLexNext(HtmlLex* l) {
    l->selfClose = false;
    l->attrs = {};
    l->text = {};
    // A run of comments is skipped in place rather than by recursing, so a
    // document that is mostly comments costs no stack.
    while (l->at < l->src.len && HtmlAtStartTag(l, l->at) &&
           l->src.s[l->at + 1] == '!') {
        // A comment runs to "-->", a doctype to the first '>'.
        bool comment = l->at + 3 < l->src.len && l->src.s[l->at + 2] == '-' &&
                       l->src.s[l->at + 3] == '-';
        l->at += comment ? 4 : 2;
        while (l->at < l->src.len) {
            if (comment) {
                if (l->at + 2 < l->src.len && l->src.s[l->at] == '-' &&
                    l->src.s[l->at + 1] == '-' && l->src.s[l->at + 2] == '>') {
                    l->at += 3;
                    break;
                }
            } else if (l->src.s[l->at] == '>') {
                l->at++;
                break;
            }
            l->at++;
        }
    }
    if (l->at >= l->src.len) {
        l->tok = HtmlTok::End;
        return;
    }
    if (HtmlAtStartTag(l, l->at)) {
        char c = l->src.s[l->at + 1];
        if (c == '/') {
            l->at += 2;
            HtmlLexName(l);
            while (l->at < l->src.len && l->src.s[l->at] != '>') {
                l->at++;
            }
            if (l->at < l->src.len) {
                l->at++;
            }
            l->tok = HtmlTok::Close;
            return;
        }
        l->at++;
        HtmlLexName(l);
        HtmlLexAttrs(l);
        l->tok = HtmlTok::Start;
        return;
    }
    int start = l->at;
    l->at++;
    while (l->at < l->src.len && !HtmlAtStartTag(l, l->at)) {
        l->at++;
    }
    l->text = Str(l->src.s + start, l->at - start);
    l->tok = HtmlTok::Text;
}

// <script> and <style> hold text that is not markup — `a < b` in a script is
// not a tag — so their content is skipped by looking for the close tag
// rather than by tokenizing it.
static void HtmlLexSkipRaw(HtmlLex* l, Str name) {
    while (l->at < l->src.len) {
        if (l->src.s[l->at] == '<' && l->at + 1 < l->src.len &&
            l->src.s[l->at + 1] == '/') {
            int save = l->at;
            l->at += 2;
            HtmlLexName(l);
            if (l->name.len == name.len &&
                memcmp(l->name.s, name.s, (size_t)name.len) == 0) {
                while (l->at < l->src.len && l->src.s[l->at] != '>') {
                    l->at++;
                }
                if (l->at < l->src.len) {
                    l->at++;
                }
                return;
            }
            l->at = save + 1;
            continue;
        }
        l->at++;
    }
}

// ─── attributes and text ──────────────────────────────────────────────────

static bool HtmlNameIs(Str n, const char* s) {
    int len = (int)strlen(s);
    return n.len == len && memcmp(n.s, s, (size_t)len) == 0;
}

// The raw slice of one attribute's value, or an empty Str. A valueless
// attribute (`<td nowrap>`) reads as empty, which is what a caller asking
// for its value wants.
static Str HtmlAttrRaw(Str attrs, const char* name) {
    int nameLen = (int)strlen(name);
    int at = 0;
    while (at < attrs.len) {
        while (at < attrs.len &&
               (HtmlIsSpace(attrs.s[at]) || attrs.s[at] == '/')) {
            at++;
        }
        int ns = at;
        while (at < attrs.len && HtmlIsNameChar(attrs.s[at])) {
            at++;
        }
        int nl = at - ns;
        if (nl == 0) {
            at++; // nothing consumed: step over the odd byte
            continue;
        }
        bool match = nl == nameLen;
        for (int i = 0; match && i < nl; i++) {
            match = HtmlLower(attrs.s[ns + i]) == name[i];
        }
        while (at < attrs.len && HtmlIsSpace(attrs.s[at])) {
            at++;
        }
        if (at >= attrs.len || attrs.s[at] != '=') {
            if (match) {
                return Str(attrs.s + ns, 0);
            }
            continue;
        }
        at++; // '='
        while (at < attrs.len && HtmlIsSpace(attrs.s[at])) {
            at++;
        }
        int vs = at;
        int vl = 0;
        if (at < attrs.len && (attrs.s[at] == '"' || attrs.s[at] == '\'')) {
            char q = attrs.s[at++];
            vs = at;
            while (at < attrs.len && attrs.s[at] != q) {
                at++;
            }
            vl = at - vs;
            if (at < attrs.len) {
                at++;
            }
        } else {
            while (at < attrs.len && !HtmlIsSpace(attrs.s[at]) &&
                   attrs.s[at] != '>') {
                at++;
            }
            vl = at - vs;
        }
        if (match) {
            return Str(attrs.s + vs, vl);
        }
    }
    return {};
}

// Entity-decoded text. `raw` is <pre>: newlines and runs of spaces stand,
// where everywhere else HTML collapses them to one space. Decoding never
// grows the text — the shortest entity is four bytes and the longest
// codepoint four — so one buffer the size of the input is always enough.
static Str HtmlDecodeText(Arena* a, Str s, bool raw) {
    if (s.len <= 0) {
        return {};
    }
    char* out = (char*)Alloc(a, s.len + 1);
    if (!out) {
        return {};
    }
    int n = 0;
    for (int i = 0; i < s.len;) {
        char c = s.s[i];
        if (!raw && HtmlIsSpace(c)) {
            if (n > 0 && out[n - 1] == ' ') {
                i++;
                continue;
            }
            out[n++] = ' ';
            i++;
            continue;
        }
        if (c == '&') {
            int j = i + 1;
            while (j < s.len && j - i < 12 && s.s[j] != ';' &&
                   !HtmlIsSpace(s.s[j])) {
                j++;
            }
            if (j < s.len && s.s[j] == ';') {
                Str dec = MdDecodeEntity(a, Str(s.s + i, j - i + 1));
                if (dec.s != s.s + i) {
                    memcpy(out + n, dec.s, (size_t)dec.len);
                    n += dec.len;
                    i = j + 1;
                    continue;
                }
            }
        }
        out[n++] = c;
        i++;
    }
    out[n] = 0;
    return Str(out, n);
}

Str HtmlAttrValue(Arena* a, Str attrs, const char* name) {
    Str raw = HtmlAttrRaw(attrs, name);
    if (raw.len <= 0) {
        return raw.s ? Str(raw.s, 0) : Str{};
    }
    return HtmlDecodeText(a, raw, true);
}

// ─── the tag vocabulary ───────────────────────────────────────────────────

// html.rs BLOCK_ELEMENTS, plus the ones it names in its own match arms.
// MdKind::Group is BlockNode::Root: a container that contributes its
// children and no box of its own.
static bool HtmlBlockKind(Str n, MdKind* kind, uint8_t* level) {
    *level = 0;
    if (HtmlNameIs(n, "p")) {
        *kind = MdKind::Paragraph;
    } else if (n.len == 2 && n.s[0] == 'h' && n.s[1] >= '1' && n.s[1] <= '6') {
        *kind = MdKind::Heading;
        *level = (uint8_t)(n.s[1] - '0');
    } else if (HtmlNameIs(n, "blockquote")) {
        *kind = MdKind::Quote;
    } else if (HtmlNameIs(n, "ul") || HtmlNameIs(n, "ol")) {
        *kind = MdKind::List;
    } else if (HtmlNameIs(n, "li")) {
        *kind = MdKind::Item;
    } else if (HtmlNameIs(n, "pre")) {
        *kind = MdKind::Code;
    } else if (HtmlNameIs(n, "table")) {
        *kind = MdKind::Table;
    } else if (HtmlNameIs(n, "tr")) {
        *kind = MdKind::Row;
    } else if (HtmlNameIs(n, "td") || HtmlNameIs(n, "th")) {
        *kind = MdKind::Cell;
    } else if (HtmlNameIs(n, "dt") || HtmlNameIs(n, "dd") ||
               HtmlNameIs(n, "summary") || HtmlNameIs(n, "figcaption")) {
        // A term, a definition and a caption are each a line of prose, so
        // they read as paragraphs rather than as anonymous containers.
        *kind = MdKind::Paragraph;
    } else if (HtmlNameIs(n, "div") || HtmlNameIs(n, "section") ||
               HtmlNameIs(n, "article") || HtmlNameIs(n, "main") ||
               HtmlNameIs(n, "header") || HtmlNameIs(n, "footer") ||
               HtmlNameIs(n, "aside") || HtmlNameIs(n, "nav") ||
               HtmlNameIs(n, "figure") || HtmlNameIs(n, "details") ||
               HtmlNameIs(n, "form") || HtmlNameIs(n, "fieldset") ||
               HtmlNameIs(n, "address") || HtmlNameIs(n, "dl") ||
               HtmlNameIs(n, "body") || HtmlNameIs(n, "html") ||
               HtmlNameIs(n, "center")) {
        *kind = MdKind::Group;
    } else {
        return false;
    }
    return true;
}

// The inline marks, which are html.rs's parse_paragraph arms.
static uint8_t HtmlInlineMark(Str n) {
    if (HtmlNameIs(n, "b") || HtmlNameIs(n, "strong")) {
        return MdBold;
    }
    if (HtmlNameIs(n, "i") || HtmlNameIs(n, "em") || HtmlNameIs(n, "cite") ||
        HtmlNameIs(n, "var")) {
        return MdItalic;
    }
    if (HtmlNameIs(n, "code") || HtmlNameIs(n, "kbd") ||
        HtmlNameIs(n, "samp") || HtmlNameIs(n, "tt")) {
        return MdCode;
    }
    if (HtmlNameIs(n, "u") || HtmlNameIs(n, "ins")) {
        return MdUnderline;
    }
    if (HtmlNameIs(n, "s") || HtmlNameIs(n, "del") || HtmlNameIs(n, "strike")) {
        return MdDel;
    }
    if (HtmlNameIs(n, "mark")) {
        return MdHighlight;
    }
    return 0;
}

static uint8_t HtmlAlignValue(Str v) {
    if (v.len >= 6 && memcmp(v.s, "center", 6) == 0) {
        return MdAlignCenter;
    }
    if (v.len >= 5 && memcmp(v.s, "right", 5) == 0) {
        return MdAlignRight;
    }
    if (v.len >= 4 && memcmp(v.s, "left", 4) == 0) {
        return MdAlignLeft;
    }
    return MdAlignDefault;
}

// A cell's alignment, from `align="center"` or from `text-align` in the
// style attribute — the two places it is written. html.rs reads width and
// height out of the same pair.
static uint8_t HtmlAlign(Arena* a, Str attrs) {
    Str v = HtmlAttrRaw(attrs, "align");
    if (v.len > 0) {
        return HtmlAlignValue(v);
    }
    Str style = HtmlAttrValue(a, attrs, "style");
    for (int i = 0; i + 10 <= style.len; i++) {
        if (memcmp(style.s + i, "text-align", 10) != 0) {
            continue;
        }
        int at = i + 10;
        while (at < style.len &&
               (HtmlIsSpace(style.s[at]) || style.s[at] == ':')) {
            at++;
        }
        return HtmlAlignValue(Str(style.s + at, style.len - at));
    }
    return MdAlignDefault;
}

// html.rs attr_width_height: the number in a width / height attribute or in
// the style beside it. A percentage is relative to something this layout
// cannot ask about here, so it reads as "no size given" and the image takes
// its own, which is what an unsized one does anyway.
static float HtmlLength(Arena* a, Str attrs, const char* name) {
    Str v = HtmlAttrValue(a, attrs, name);
    if (v.len <= 0) {
        Str style = HtmlAttrValue(a, attrs, "style");
        int nameLen = (int)strlen(name);
        for (int i = 0; i + nameLen <= style.len; i++) {
            if (memcmp(style.s + i, name, (size_t)nameLen) != 0) {
                continue;
            }
            int at = i + nameLen;
            while (at < style.len &&
                   (HtmlIsSpace(style.s[at]) || style.s[at] == ':')) {
                at++;
            }
            v = Str(style.s + at, style.len - at);
            break;
        }
    }
    float n = 0;
    int i = 0;
    bool any = false;
    while (i < v.len && v.s[i] >= '0' && v.s[i] <= '9') {
        n = n * 10 + (float)(v.s[i] - '0');
        any = true;
        i++;
    }
    if (i < v.len && v.s[i] == '.') {
        i++;
        float scale = 0.1f;
        while (i < v.len && v.s[i] >= '0' && v.s[i] <= '9') {
            n += (float)(v.s[i] - '0') * scale;
            scale *= 0.1f;
            any = true;
            i++;
        }
    }
    if (!any || (i < v.len && v.s[i] == '%')) {
        return 0;
    }
    return n;
}

HtmlInlineTag HtmlParseInlineTag(Arena* a, Str tag) {
    HtmlInlineTag t;
    HtmlLex l;
    l.src = tag;
    HtmlLexNext(&l);
    if (l.tok != HtmlTok::Start && l.tok != HtmlTok::Close) {
        return t;
    }
    t.close = l.tok == HtmlTok::Close;
    if (HtmlNameIs(l.name, "br")) {
        t.known = !t.close;
        t.isBreak = t.known;
        return t;
    }
    if (HtmlNameIs(l.name, "img")) {
        t.known = !t.close;
        t.isImage = t.known;
        if (t.known) {
            t.alt = HtmlAttrValue(a, l.attrs, "alt");
            t.src = HtmlAttrValue(a, l.attrs, "src");
            t.width = HtmlLength(a, l.attrs, "width");
            t.height = HtmlLength(a, l.attrs, "height");
        }
        return t;
    }
    if (HtmlNameIs(l.name, "a")) {
        t.known = true;
        t.mark = MdLink;
        if (!t.close) {
            t.href = HtmlAttrValue(a, l.attrs, "href");
        }
        return t;
    }
    t.mark = HtmlInlineMark(l.name);
    t.known = t.mark != 0;
    return t;
}

// ─── the tree builder ─────────────────────────────────────────────────────

// One open element. A block pushed a node; an inline one only added marks.
// Closing restores whichever it was, which is also what makes an unclosed
// tag harmless: the close of an ancestor pops everything above it.
struct HtmlOpen {
    MdNode* node = nullptr;
    Str name = {};
    uint8_t mark = 0;
    bool hadHref = false;
    Str prevHref = {};
    bool head = false;
    bool prevHead = false;
    bool raw = false;
    bool prevRaw = false;
};

struct HtmlBuild {
    Arena* a = nullptr;
    // The block that receives children.
    MdNode* cur = nullptr;
    // The paragraph opened for text that arrived with no block around it —
    // html.rs's `paragraph` accumulator, consumed the same way.
    MdNode* para = nullptr;
    uint8_t marks = 0;
    Str href = {};
    bool inHead = false;
    bool raw = false;
    // Deep enough for any document a reader view shows; past it the tags are
    // still tokenized, they just stop nesting.
    HtmlOpen stack[64];
    int depth = 0;
};

static MdNode* HtmlNewNode(HtmlBuild* b, MdKind k) {
    MdNode* n = ArenaNew<MdNode>(b->a);
    n->kind = k;
    n->parent = b->cur;
    if (b->cur->last) {
        b->cur->last->next = n;
    } else {
        b->cur->first = n;
    }
    b->cur->last = n;
    return n;
}

static void HtmlPush(HtmlBuild* b, MdNode* n, Str name) {
    if (b->depth >= (int)(sizeof(b->stack) / sizeof(b->stack[0]))) {
        return;
    }
    HtmlOpen& o = b->stack[b->depth++];
    o = HtmlOpen{};
    o.node = n;
    o.name = name;
    if (n) {
        b->cur = n;
        b->para = nullptr;
    }
}

// The node text goes into. A block that holds runs takes it directly;
// anything else opens a paragraph, which is html.rs wrapping loose text in a
// BlockNode::Paragraph before the next block element starts.
static MdNode* HtmlTextTarget(HtmlBuild* b) {
    MdKind k = b->cur->kind;
    if (k == MdKind::Paragraph || k == MdKind::Heading || k == MdKind::Cell ||
        k == MdKind::Code || k == MdKind::Item) {
        return b->cur;
    }
    if (!b->para) {
        b->para = HtmlNewNode(b, MdKind::Paragraph);
    }
    return b->para;
}

static void HtmlAddRun(HtmlBuild* b, Str text) {
    if (text.len <= 0) {
        return;
    }
    MdNode* n = HtmlTextTarget(b);
    // A run that only differs by where it came from is the same run: merging
    // keeps the common paragraph down to one, which is the shape the renderer
    // hands whole to the text engine instead of splitting into words.
    MdRun* last = n->runLast;
    if (last && last->marks == b->marks && last->href.s == b->href.s &&
        last->text.s + last->text.len == text.s) {
        last->text.len += text.len;
        return;
    }
    MdRun* r = ArenaNew<MdRun>(b->a);
    r->text = text;
    r->marks = b->marks;
    r->href = b->href;
    if (n->runLast) {
        n->runLast->next = r;
    } else {
        n->runFirst = r;
    }
    n->runLast = r;
}

// Whether the run list text would land in is still empty, without opening a
// paragraph to find out.
static bool HtmlTargetEmpty(HtmlBuild* b) {
    MdKind k = b->cur->kind;
    if (k == MdKind::Paragraph || k == MdKind::Heading || k == MdKind::Cell ||
        k == MdKind::Code || k == MdKind::Item) {
        return b->cur->runFirst == nullptr;
    }
    return !b->para || b->para->runFirst == nullptr;
}

// html.rs push_image: an ImageNode in the paragraph, carrying whatever marks
// are in force — an <img> inside an <a> is a link, the way image.link is
// there.
static void HtmlAddImage(HtmlBuild* b, Str src, Str alt, float w, float h) {
    if (src.len <= 0) {
        // html.rs warns and drops an image with no src; so does this.
        return;
    }
    MdNode* n = HtmlTextTarget(b);
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

static void HtmlText(HtmlBuild* b, Str raw) {
    Str s = HtmlDecodeText(b->a, raw, b->raw);
    if (s.len <= 0) {
        return;
    }
    if (!b->raw) {
        // Whitespace between two block elements is layout, not content, and
        // a paragraph never opens with the space that followed its tag.
        bool empty = HtmlTargetEmpty(b);
        if (s.len == 1 && s.s[0] == ' ') {
            if (empty) {
                return;
            }
        } else if (s.s[0] == ' ' && empty) {
            s = Str(s.s + 1, s.len - 1);
        }
    } else if (!b->cur->runFirst && s.s[0] == '\n') {
        // <pre> swallows the newline that follows its tag.
        s = Str(s.s + 1, s.len - 1);
    }
    HtmlAddRun(b, s);
}

static void HtmlClose(HtmlBuild* b, Str name) {
    int found = -1;
    for (int i = b->depth - 1; i >= 0; i--) {
        if (b->stack[i].name.len == name.len &&
            memcmp(b->stack[i].name.s, name.s, (size_t)name.len) == 0) {
            found = i;
            break;
        }
    }
    if (found < 0) {
        return;
    }
    for (int i = b->depth - 1; i >= found; i--) {
        HtmlOpen& o = b->stack[i];
        if (o.node) {
            b->cur = o.node->parent ? o.node->parent : b->cur;
            b->para = nullptr;
        }
        if (o.mark) {
            b->marks = (uint8_t)(b->marks & ~o.mark);
        }
        if (o.hadHref) {
            b->href = o.prevHref;
        }
        if (o.head) {
            b->inHead = o.prevHead;
        }
        if (o.raw) {
            b->raw = o.prevRaw;
        }
    }
    b->depth = found;
}

// <ol start="3">. A value that is not a number leaves the list at 1, the way
// an absent one does.
static int HtmlListStart(Str v) {
    if (v.len <= 0) {
        return 1;
    }
    int n = 0;
    for (int i = 0; i < v.len; i++) {
        if (v.s[i] < '0' || v.s[i] > '9') {
            return 1;
        }
        n = n * 10 + (v.s[i] - '0');
    }
    return n;
}

static void HtmlStart(HtmlBuild* b, HtmlLex* l) {
    Str name = StrDup(b->a, l->name);
    MdKind kind = MdKind::Group;
    uint8_t level = 0;

    if (HtmlNameIs(l->name, "br")) {
        HtmlAddRun(b, StrL("\n"));
        return;
    }
    if (HtmlNameIs(l->name, "hr")) {
        b->para = nullptr;
        HtmlNewNode(b, MdKind::Rule);
        return;
    }
    if (HtmlNameIs(l->name, "img")) {
        HtmlAddImage(b, HtmlAttrValue(b->a, l->attrs, "src"),
                     HtmlAttrValue(b->a, l->attrs, "alt"),
                     HtmlLength(b->a, l->attrs, "width"),
                     HtmlLength(b->a, l->attrs, "height"));
        return;
    }
    if (HtmlNameIs(l->name, "thead") || HtmlNameIs(l->name, "tbody") ||
        HtmlNameIs(l->name, "tfoot")) {
        // Not a node of its own: it only says which rows are header rows,
        // exactly as MD_BLOCK_THEAD does on the markdown side.
        bool head = HtmlNameIs(l->name, "thead");
        HtmlPush(b, nullptr, name);
        if (b->depth > 0) {
            HtmlOpen& o = b->stack[b->depth - 1];
            o.head = true;
            o.prevHead = b->inHead;
        }
        b->inHead = head;
        return;
    }

    if (HtmlBlockKind(l->name, &kind, &level)) {
        // <p>text<div>..</div> is not nesting: an open paragraph gives way to
        // the block that follows it.
        if (b->cur->kind == MdKind::Paragraph) {
            for (int i = b->depth - 1; i >= 0; i--) {
                if (b->stack[i].node == b->cur) {
                    HtmlClose(b, b->stack[i].name);
                    break;
                }
            }
        }
        MdNode* n = HtmlNewNode(b, kind);
        n->level = level;
        if (kind == MdKind::List) {
            n->ordered = HtmlNameIs(l->name, "ol");
            n->start = HtmlListStart(HtmlAttrValue(b->a, l->attrs, "start"));
        } else if (kind == MdKind::Row) {
            n->head = b->inHead;
        } else if (kind == MdKind::Cell) {
            n->align = HtmlAlign(b->a, l->attrs);
            if (HtmlNameIs(l->name, "th") && n->parent) {
                n->parent->head = true;
            }
        }
        HtmlPush(b, n, name);
        if (kind == MdKind::Code && b->depth > 0) {
            HtmlOpen& o = b->stack[b->depth - 1];
            o.raw = true;
            o.prevRaw = b->raw;
            b->raw = true;
        }
        if (l->selfClose) {
            HtmlClose(b, name);
        }
        return;
    }

    // <code class="language-cpp"> inside a <pre> names the fence's language
    // rather than marking a span, which is the shape every markdown-rendered
    // code block on the web has.
    if (b->cur->kind == MdKind::Code && HtmlNameIs(l->name, "code")) {
        Str cls = HtmlAttrValue(b->a, l->attrs, "class");
        if (cls.len > 9 && memcmp(cls.s, "language-", 9) == 0) {
            b->cur->lang = Str(cls.s + 9, cls.len - 9);
        }
        HtmlPush(b, nullptr, name);
        return;
    }

    uint8_t mark = HtmlInlineMark(l->name);
    bool link = HtmlNameIs(l->name, "a");
    HtmlPush(b, nullptr, name);
    if (b->depth <= 0) {
        return;
    }
    HtmlOpen& o = b->stack[b->depth - 1];
    if (link) {
        o.mark = MdLink;
        o.hadHref = true;
        o.prevHref = b->href;
        b->href = HtmlAttrValue(b->a, l->attrs, "href");
        b->marks = (uint8_t)(b->marks | MdLink);
    } else if (mark) {
        o.mark = mark;
        b->marks = (uint8_t)(b->marks | mark);
    }
    if (l->selfClose) {
        HtmlClose(b, name);
    }
}

void HtmlParseInto(Arena* a, MdNode* parent, Str source) {
    if (!parent || !source.s || source.len <= 0) {
        return;
    }
    HtmlBuild b;
    b.a = a;
    b.cur = parent;

    HtmlLex l;
    l.src = source;
    for (;;) {
        HtmlLexNext(&l);
        if (l.tok == HtmlTok::End) {
            break;
        }
        if (l.tok == HtmlTok::Text) {
            HtmlText(&b, l.text);
            continue;
        }
        if (l.tok == HtmlTok::Close) {
            HtmlClose(&b, l.name);
            continue;
        }
        // <head> and <title> are metadata; <style> and <script> are the two
        // html.rs drops by name. All four take their content with them.
        if (HtmlNameIs(l.name, "head") || HtmlNameIs(l.name, "title") ||
            HtmlNameIs(l.name, "script") || HtmlNameIs(l.name, "style")) {
            Str name = StrDup(a, l.name);
            if (!l.selfClose) {
                HtmlLexSkipRaw(&l, name);
            }
            continue;
        }
        HtmlStart(&b, &l);
    }
}

MdNode* HtmlParse(Arena* a, Str source) {
    MdNode* doc = ArenaNew<MdNode>(a);
    doc->kind = MdKind::Doc;
    HtmlParseInto(a, doc, source);
    return doc;
}

} // namespace component
} // namespace gpui
