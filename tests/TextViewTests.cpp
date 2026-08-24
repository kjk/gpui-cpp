/* Ports of the parse tests in crates/ui/src/text — format/markdown.rs and
   format/html.rs both end at a BlockNode tree, and these check the MdNode
   tree ui/text.cpp and ui/html.cpp build in its place. */

#include "Test.h"

using namespace gpui::component;

// The n-th child of `n`, or null.
static MdNode* Child(MdNode* n, int ix) {
    if (!n) {
        return nullptr;
    }
    for (MdNode* c = n->first; c; c = c->next) {
        if (ix-- == 0) {
            return c;
        }
    }
    return nullptr;
}

static int Children(MdNode* n) {
    int count = 0;
    for (MdNode* c = n ? n->first : nullptr; c; c = c->next) {
        count++;
    }
    return count;
}

// Every run of a node concatenated, which is the text it shows.
static Str NodeText(Arena* a, MdNode* n) {
    int len = 0;
    for (MdRun* r = n ? n->runFirst : nullptr; r; r = r->next) {
        len += r->text.len;
    }
    char* buf = (char*)Alloc(a, len + 1);
    int at = 0;
    for (MdRun* r = n ? n->runFirst : nullptr; r; r = r->next) {
        memcpy(buf + at, r->text.s, (size_t)r->text.len);
        at += r->text.len;
    }
    buf[at] = 0;
    return Str(buf, at);
}

static bool TextIs(Arena* a, MdNode* n, const char* want) {
    Str got = NodeText(a, n);
    int len = (int)strlen(want);
    return got.len == len &&
           (len == 0 || memcmp(got.s, want, (size_t)len) == 0);
}

// The marks on the run covering `needle`, or 0xff when no run holds it.
static uint8_t MarksOf(MdNode* n, const char* needle) {
    int len = (int)strlen(needle);
    for (MdRun* r = n ? n->runFirst : nullptr; r; r = r->next) {
        for (int i = 0; i + len <= r->text.len; i++) {
            if (memcmp(r->text.s + i, needle, (size_t)len) == 0) {
                return r->marks;
            }
        }
    }
    return 0xff;
}

static Str HrefOf(MdNode* n, const char* needle) {
    int len = (int)strlen(needle);
    for (MdRun* r = n ? n->runFirst : nullptr; r; r = r->next) {
        for (int i = 0; i + len <= r->text.len; i++) {
            if (memcmp(r->text.s + i, needle, (size_t)len) == 0) {
                return r->href;
            }
        }
    }
    return {};
}

static bool StrIs(Str s, const char* want) {
    int len = (int)strlen(want);
    return s.len == len && (len == 0 || memcmp(s.s, want, (size_t)len) == 0);
}

// ─── markdown ─────────────────────────────────────────────────────────────

static void TestMarkdownBlocks(Arena* a) {
    MdNode* doc = MdParse(a, StrL("# Title\n\nSome *text*.\n\n- one\n- two\n"));
    utassert(Children(doc) == 3);
    MdNode* h = Child(doc, 0);
    utassert(h->kind == MdKind::Heading);
    utassert(h->level == 1);
    utassert(TextIs(a, h, "Title"));
    MdNode* p = Child(doc, 1);
    utassert(p->kind == MdKind::Paragraph);
    utassert(MarksOf(p, "text") == MdItalic);
    MdNode* list = Child(doc, 2);
    utassert(list->kind == MdKind::List);
    utassert(!list->ordered);
    utassert(Children(list) == 2);
    // An item holds blocks: mdast gives even a tight list item a paragraph
    // of its own.
    utassert(TextIs(a, Child(Child(list, 1), 0), "two"));
}

// GFM task list items: mdast reports the `[x]` as the item's `checked` and
// takes the marker off the text, which is what markdown.rs carries onto the
// BlockNode.
static void TestMarkdownTaskList(Arena* a) {
    MdNode* doc = MdParse(a, StrL("- [x] done\n- [ ] todo\n- plain\n"));
    MdNode* list = Child(doc, 0);
    utassert(list->kind == MdKind::List);
    MdNode* done = Child(list, 0);
    utassert(done->hasCheck && done->checked);
    utassert(TextIs(a, Child(done, 0), "done"));
    MdNode* todo = Child(list, 1);
    utassert(todo->hasCheck && !todo->checked);
    utassert(TextIs(a, Child(todo, 0), "todo"));
    // An item with no checkbox carries neither half of the Option.
    MdNode* plain = Child(list, 2);
    utassert(!plain->hasCheck && !plain->checked);
}

// The delimiter row's colons, which node.rs render_wrap_table aligns each
// column by. mdast reports them once per column, as `Table::align`.
static void TestMarkdownTableAlign(Arena* a) {
    MdNode* doc = MdParse(a, StrL("| a | b | c |\n"
                                  "|:--|:-:|--:|\n"
                                  "| 1 | 2 | 3 |\n"));
    MdNode* table = Child(doc, 0);
    utassert(table->kind == MdKind::Table);
    MdNode* head = Child(table, 0);
    utassert(head->head);
    utassert(Child(head, 0)->align == MdAlignLeft);
    utassert(Child(head, 1)->align == MdAlignCenter);
    utassert(Child(head, 2)->align == MdAlignRight);
    MdNode* body = Child(table, 1);
    utassert(!body->head);
    utassert(Child(body, 2)->align == MdAlignRight);
}

// Inline HTML inside a paragraph: the parser hands the tags over as mdast
// Html nodes and text.cpp turns them into the marks html5ever would have
// produced.
static void TestMarkdownInlineHtml(Arena* a) {
    MdNode* doc = MdParse(
        a, StrL("Plain <b>bold</b> and <a href=\"http://x/\">link</a>.\n"));
    MdNode* p = Child(doc, 0);
    utassert(p->kind == MdKind::Paragraph);
    utassert(MarksOf(p, "Plain") == 0);
    utassert(MarksOf(p, "bold") == MdBold);
    utassert(MarksOf(p, "link") == MdLink);
    utassert(StrIs(HrefOf(p, "link"), "http://x/"));
    // The mark ends with the tag: what follows is unmarked again.
    utassert(MarksOf(p, "and") == 0);
}

// A raw HTML block is parsed rather than dropped — Rust hands the same node
// to format::html from markdown_ext.rs.
static void TestMarkdownHtmlBlock(Arena* a) {
    MdNode* doc = MdParse(a, StrL("Before\n\n<div>\n  <p>Inside</p>\n"
                                  "</div>\n\nAfter\n"));
    utassert(Children(doc) == 3);
    utassert(TextIs(a, Child(doc, 0), "Before"));
    MdNode* html = Child(doc, 1);
    utassert(html->kind == MdKind::Html);
    utassert(html->runFirst == nullptr);
    MdNode* div = Child(html, 0);
    utassert(div->kind == MdKind::Group);
    utassert(TextIs(a, Child(div, 0), "Inside"));
    utassert(TextIs(a, Child(doc, 2), "After"));
}

// ─── html ─────────────────────────────────────────────────────────────────

static void TestHtmlBlocks(Arena* a) {
    MdNode* doc = HtmlParse(a, StrL("<html><head><title>t</title></head>"
                                    "<body><h2>Head</h2><p>Body text</p>"
                                    "<script>if (a < b) {}</script>"
                                    "</body></html>"));
    MdNode* body = Child(Child(doc, 0), 0);
    utassert(body->kind == MdKind::Group);
    MdNode* h = Child(body, 0);
    utassert(h->kind == MdKind::Heading);
    utassert(h->level == 2);
    utassert(TextIs(a, h, "Head"));
    MdNode* p = Child(body, 1);
    utassert(TextIs(a, p, "Body text"));
    // <head>, <title>, <script> and <style> take their content with them.
    utassert(Children(body) == 2);
}

static void TestHtmlInlineMarks(Arena* a) {
    MdNode* doc =
        HtmlParse(a, StrL("<p>a <b>b</b> <i>i</i> <code>c</code> <s>s</s> "
                          "<mark>m</mark> <a href='/go'>go</a></p>"));
    MdNode* p = Child(doc, 0);
    utassert(p->kind == MdKind::Paragraph);
    utassert(MarksOf(p, "b") == MdBold);
    utassert(MarksOf(p, "i") == MdItalic);
    utassert(MarksOf(p, "c") == MdCode);
    utassert(MarksOf(p, "s") == MdDel);
    utassert(MarksOf(p, "m") == MdHighlight);
    utassert(MarksOf(p, "go") == MdLink);
    utassert(StrIs(HrefOf(p, "go"), "/go"));
}

// Nested marks: html.rs merges the child's marks with the parent's, so the
// inner text carries both.
static void TestHtmlNestedMarks(Arena* a) {
    MdNode* doc = HtmlParse(a, StrL("<p><b>bold <i>both</i></b></p>"));
    MdNode* p = Child(doc, 0);
    utassert(MarksOf(p, "bold") == MdBold);
    utassert(MarksOf(p, "both") == (MdBold | MdItalic));
}

// Whitespace between block elements is layout, not content; inside a
// paragraph a run of it collapses to one space.
static void TestHtmlWhitespace(Arena* a) {
    MdNode* doc = HtmlParse(a, StrL("<div>\n   <p>one\n   two</p>\n</div>"));
    MdNode* div = Child(doc, 0);
    utassert(Children(div) == 1);
    utassert(TextIs(a, Child(div, 0), "one two"));
}

static void TestHtmlEntities(Arena* a) {
    MdNode* doc = HtmlParse(a, StrL("<p>a &amp; b &lt;c&gt; &#65; &nope;</p>"));
    utassert(TextIs(a, Child(doc, 0), "a & b <c> A &nope;"));
}

static void TestHtmlList(Arena* a) {
    MdNode* doc =
        HtmlParse(a, StrL("<ol start=\"3\"><li>one</li><li>two</li></ol>"));
    MdNode* list = Child(doc, 0);
    utassert(list->kind == MdKind::List);
    utassert(list->ordered);
    utassert(list->start == 3);
    utassert(Children(list) == 2);
    utassert(TextIs(a, Child(list, 0), "one"));
}

// <pre> keeps its text as typed, and the <code class> inside it names the
// language the way a fenced block's info string does.
static void TestHtmlPre(Arena* a) {
    MdNode* doc =
        HtmlParse(a, StrL("<pre><code class=\"language-cpp\">int a;\n  int b;\n"
                          "</code></pre>"));
    MdNode* code = Child(doc, 0);
    utassert(code->kind == MdKind::Code);
    utassert(StrIs(code->lang, "cpp"));
    utassert(TextIs(a, code, "int a;\n  int b;\n"));
}

static void TestHtmlTable(Arena* a) {
    MdNode* doc = HtmlParse(
        a, StrL("<table><thead><tr><th>h</th></tr></thead>"
                "<tbody><tr><td align=\"center\">c</td>"
                "<td style=\"text-align: right\">r</td></tr></tbody></table>"));
    MdNode* table = Child(doc, 0);
    utassert(table->kind == MdKind::Table);
    utassert(Children(table) == 2);
    MdNode* head = Child(table, 0);
    utassert(head->head);
    MdNode* row = Child(table, 1);
    utassert(!row->head);
    utassert(Child(row, 0)->align == MdAlignCenter);
    utassert(Child(row, 1)->align == MdAlignRight);
}

// An <img> has no loader behind it, so it contributes its alt text — the same
// place a markdown ![alt](url) lands.
static void TestHtmlImageAlt(Arena* a) {
    MdNode* doc = HtmlParse(a, StrL("<p>see <img src=\"a.png\" alt=\"a cat\">"
                                    " here</p>"));
    utassert(TextIs(a, Child(doc, 0), "see a cat here"));
}

// A tag left open is closed by its ancestor, and a stray close tag that
// matches nothing is ignored.
static void TestHtmlUnbalanced(Arena* a) {
    MdNode* doc = HtmlParse(a, StrL("<div><p>one<b>two</div></i><p>three</p>"));
    MdNode* div = Child(doc, 0);
    utassert(div->kind == MdKind::Group);
    utassert(TextIs(a, Child(div, 0), "onetwo"));
    utassert(TextIs(a, Child(doc, 1), "three"));
}

static void TestHtmlComments(Arena* a) {
    MdNode* doc =
        HtmlParse(a, StrL("<!doctype html><!-- <p>hidden</p> --><p>shown</p>"));
    utassert(Children(doc) == 1);
    utassert(TextIs(a, Child(doc, 0), "shown"));
}

// <br> is a hard break inside the flow, which the renderer starts a new row
// on; the tree carries it as a newline in the run.
static void TestHtmlBreak(Arena* a) {
    MdNode* doc = HtmlParse(a, StrL("<p>one<br>two</p>"));
    utassert(TextIs(a, Child(doc, 0), "one\ntwo"));
}

// The run covering `needle`, or the first image run when `needle` is null.
static MdRun* ImageRunOf(MdNode* n) {
    for (MdRun* r = n ? n->runFirst : nullptr; r; r = r->next) {
        if (r->imgSrc.len > 0) {
            return r;
        }
    }
    return nullptr;
}

// node.rs InlineNode::image: a markdown image is a run of its own, carrying
// the source and the alt text beside the words.
static void TestMarkdownImage(Arena* a) {
    MdNode* doc = MdParse(a, StrL("see ![a cat](cat.png) here\n"));
    MdNode* p = Child(doc, 0);
    MdRun* img = ImageRunOf(p);
    utassert(img != nullptr);
    utassert(StrIs(img->imgSrc, "cat.png"));
    utassert(StrIs(img->text, "a cat"));
    // The words around it are still their own runs, in order.
    utassert(TextIs(a, p, "see a cat here"));
    // An image inside a link is a link, the way ImageNode::link is.
    MdNode* linked = MdParse(a, StrL("[![alt](c.png)](https://x/)\n"));
    MdRun* r = ImageRunOf(Child(linked, 0));
    utassert(r != nullptr);
    utassert((r->marks & MdLink) != 0);
    utassert(StrIs(r->href, "https://x/"));
}

// html.rs attr_width_height: the size the tag gives, in pixels. A percentage
// is not a size this layout can use, so it reads as none.
static void TestHtmlImage(Arena* a) {
    MdNode* doc = HtmlParse(
        a, StrL("<p>a <img src=\"x.png\" alt=\"alt\" width=\"60\" "
                "height=\"40\"> b</p>"));
    MdRun* img = ImageRunOf(Child(doc, 0));
    utassert(img != nullptr);
    utassert(StrIs(img->imgSrc, "x.png"));
    utassert(StrIs(img->text, "alt"));
    utassert(img->imgW == 60 && img->imgH == 40);

    MdNode* pct =
        HtmlParse(a, StrL("<img src=\"y.png\" style=\"width: 50%\">"));
    MdRun* r = ImageRunOf(Child(pct, 0));
    utassert(r != nullptr);
    utassert(r->imgW == 0);

    // html.rs drops an image with no src; so does this.
    MdNode* nosrc = HtmlParse(a, StrL("<p><img alt=\"x\"></p>"));
    utassert(ImageRunOf(Child(nosrc, 0)) == nullptr);
}

// gpui/image.h: what a src may name. A URL is somewhere this tree cannot
// reach, so it never reaches the cache.
static void TestImageSrc() {
    utassert(ImageSrcIsLocal(StrL("logo.png")));
    utassert(ImageSrcIsLocal(StrL("icons/logo.png")));
    utassert(ImageSrcIsLocal(StrL("data:image/png;base64,iVBORw0KGgo=")));
    utassert(!ImageSrcIsLocal(StrL("https://example.com/a.png")));
    utassert(!ImageSrcIsLocal(StrL("http://example.com/a.png")));
    utassert(!ImageSrcIsLocal(StrL("")));
}

// ─── SelectionFormat::Source ──────────────────────────────────────────────
//
// Ports of node.rs's own reconstruct tests — reconstruct_markdown_wraps_
// marked_runs, reconstruct_markdown_emits_unmarked_text_verbatim, and the
// selected_source cases for headings, blockquotes, code blocks and tables.
// Rust rebuilds the markdown by walking the BlockNode tree; here the walk
// happens as the tree is built and each painted run carries its piece of it,
// so what these drive is the other end: CopyTextHitsIn putting the pieces
// back together over a frame's registered runs.

// A frame's text registrations, built by hand the way a paint pass builds
// them.
struct SrcDoc {
    PaintCtx ctx;

    void Run(const char* text, const SelSource* src, bool join) {
        TextHit h;
        h.bounds = {0, 0, 100, 20};
        h.text = Str((char*)text);
        h.font = 14;
        h.maxW = 100;
        h.docOff = ctx.textDocLen;
        h.src = src;
        h.join = join;
        ctx.texts.Append(h);
        // The gap of one between runs, which is what the copier's document
        // order leaves room for.
        ctx.textDocLen += h.text.len + 1;
    }

    // An inline image: a run with no text of its own, holding one place in
    // the document order.
    void Image(const SelSource* src, bool join) {
        TextHit h;
        h.bounds = {0, 0, 20, 20};
        h.font = 14;
        h.docOff = ctx.textDocLen;
        h.src = src;
        h.join = join;
        h.atom = true;
        ctx.texts.Append(h);
        ctx.textDocLen += 1;
    }
};

// The whole document, copied in `fmt`.
static Str SrcCopy(SrcDoc* d, SelectionFormat fmt, char* buf, int cap) {
    // One short of the gap after the last run, so nothing reaches past it.
    int n = CopyTextHitsIn(&d->ctx, 0, d->ctx.textDocLen - 1, -1, buf, cap,
                           fmt);
    return Str(buf, n);
}

static bool SrcIs(Str got, const char* want) {
    Str w = Str((char*)want);
    return got.len == w.len && memcmp(got.s, w.s, (size_t)w.len) == 0;
}

// A mark group split over several word elements wraps once, not per word —
// which is what reconstruct_markdown gets from walking mark ranges rather
// than words.
static void TestSourceMarks() {
    char buf[512];
    SelBlock para = {};
    SelSource bold = {StrL("**"), StrL("**"), &para};
    SelSource plain = {{}, {}, &para};
    SrcDoc d;
    d.Run("one ", &bold, false);
    d.Run("two ", &bold, true);
    d.Run("three", &plain, true);
    utassert(SrcIs(SrcCopy(&d, SelectionFormat::Source, buf, sizeof(buf)),
                   "**one two **three"));
    // The same runs in Plain are the text as rendered, on one line: a
    // paragraph is one InlineState.text in Rust however it is copied.
    utassert(SrcIs(SrcCopy(&d, SelectionFormat::Plain, buf, sizeof(buf)),
                   "one two three"));
}

// reconstruct_markdown: a partial selection inside a marked run still wraps
// the slice.
static void TestSourcePartialMark() {
    char buf[512];
    SelBlock para = {};
    SelSource bold = {StrL("**"), StrL("**"), &para};
    SrcDoc d;
    d.Run("bold", &bold, false);
    int n = CopyTextHitsIn(&d.ctx, 1, 3, -1, buf, sizeof(buf),
                           SelectionFormat::Source);
    utassert(SrcIs(Str(buf, n), "**ol**"));
}

// reconstruct_markdown_emits_unmarked_text_verbatim, and a link's tail.
static void TestSourceCodeAndLink() {
    char buf[512];
    SelBlock para = {};
    SelSource plain = {{}, {}, &para};
    SelSource code = {StrL("`"), StrL("`"), &para};
    SelSource link = {StrL("["), StrL("](https://x.dev)"), &para};
    SrcDoc d;
    d.Run("a ", &plain, false);
    d.Run("b", &code, true);
    d.Run(" c ", &plain, true);
    d.Run("home", &link, true);
    utassert(SrcIs(SrcCopy(&d, SelectionFormat::Source, buf, sizeof(buf)),
                   "a `b` c [home](https://x.dev)"));
}

// A selected heading round-trips with its marker, and the paragraph under it
// starts a line of its own.
static void TestSourceHeading() {
    char buf[512];
    SelBlock head = {StrL("## "), {}, {}, false};
    SelBlock para = {};
    SelSource h = {{}, {}, &head};
    SelSource p = {{}, {}, &para};
    SrcDoc d;
    d.Run("Title", &h, false);
    d.Run("body", &p, false);
    utassert(SrcIs(SrcCopy(&d, SelectionFormat::Source, buf, sizeof(buf)),
                   "## Title\nbody"));
    utassert(SrcIs(SrcCopy(&d, SelectionFormat::Plain, buf, sizeof(buf)),
                   "Title\nbody"));
}

// Every line of a blockquote carries its prefix, including the ones inside a
// run that holds its own line breaks.
static void TestSourceBlockquote() {
    char buf[512];
    SelBlock q1 = {StrL("> "), {}, StrL("> "), false};
    SelBlock q2 = {StrL("> "), {}, StrL("> "), false};
    SelSource a = {{}, {}, &q1};
    SelSource b = {{}, {}, &q2};
    SrcDoc d;
    d.Run("first", &a, false);
    d.Run("second\nthird", &b, false);
    utassert(SrcIs(SrcCopy(&d, SelectionFormat::Source, buf, sizeof(buf)),
                   "> first\n> second\n> third"));
}

// code_block.selected_source: the code comes back fenced, with the block's
// language on the opening fence.
static void TestSourceCodeBlock() {
    char buf[512];
    SelBlock fence = {StrL("```rust\n"), StrL("\n```"), {}, false};
    SelSource tok = {{}, {}, &fence};
    SrcDoc d;
    d.Run("let x", &tok, false);
    d.Run(" = 1;", &tok, true);
    utassert(SrcIs(SrcCopy(&d, SelectionFormat::Source, buf, sizeof(buf)),
                   "```rust\nlet x = 1;\n```"));
}

// table_selected_source: the row is piped and the alignment row follows the
// header. In Plain the cells of a row are joined with a space.
static void TestSourceTable() {
    char buf[512];
    SelBlock h0 = {StrL("| "), StrL(" "), {}, false};
    SelBlock h1 = {StrL("| "), StrL(" |\n| :-- | :-: |"), {}, true};
    SelBlock b0 = {StrL("| "), StrL(" "), {}, false};
    SelBlock b1 = {StrL("| "), StrL(" |"), {}, true};
    SelSource s0 = {{}, {}, &h0};
    SelSource s1 = {{}, {}, &h1};
    SelSource s2 = {{}, {}, &b0};
    SelSource s3 = {{}, {}, &b1};
    SrcDoc d;
    d.Run("Name", &s0, false);
    d.Run("Qty", &s1, false);
    d.Run("Nut", &s2, false);
    d.Run("3", &s3, false);
    utassert(SrcIs(SrcCopy(&d, SelectionFormat::Source, buf, sizeof(buf)),
                   "| Name | Qty |\n| :-- | :-: |\n| Nut | 3 |"));
    utassert(SrcIs(SrcCopy(&d, SelectionFormat::Plain, buf, sizeof(buf)),
                   "Name Qty\nNut 3"));
}

// A list item's marker is the markdown one, not the bullet glyph it draws
// with, and the lines under it are indented by the marker's width.
static void TestSourceList() {
    char buf[512];
    SelBlock item = {StrL("- "), {}, StrL("  "), false};
    SelBlock nested = {StrL("  - "), {}, StrL("    "), false};
    SelSource a = {{}, {}, &item};
    SelSource b = {{}, {}, &nested};
    SrcDoc d;
    d.Run("first", &a, false);
    d.Run("under", &b, false);
    utassert(SrcIs(SrcCopy(&d, SelectionFormat::Source, buf, sizeof(buf)),
                   "- first\n  - under"));
}

// A task list item: list_selected_source puts the checkbox after the marker
// and indents the lines under it by the marker alone, so the `[x] ` stays on
// the first line.
static void TestSourceTaskList() {
    char buf[512];
    SelBlock done = {StrL("- [x] "), {}, StrL("  "), false};
    SelBlock todo = {StrL("- [ ] "), {}, StrL("  "), false};
    SelSource a = {{}, {}, &done};
    SelSource b = {{}, {}, &todo};
    SrcDoc d;
    d.Run("shipped", &a, false);
    d.Run("pending", &b, false);
    utassert(SrcIs(SrcCopy(&d, SelectionFormat::Source, buf, sizeof(buf)),
                   "- [x] shipped\n- [ ] pending"));
    // The rendered text is the item's words: the checkbox is drawn, not
    // written.
    utassert(SrcIs(SrcCopy(&d, SelectionFormat::Plain, buf, sizeof(buf)),
                   "shipped\npending"));
}

// node.rs selected_source: an inline image is emitted when the selection runs
// into it — the run before it selected to its end, the run after it from its
// beginning — and copies as nothing in Plain, since Paragraph::text lays the
// children's text end to end and an image child has none.
static void TestSourceImage() {
    char buf[512];
    SelBlock para = {};
    SelSource plain = {{}, {}, &para};
    SelSource img = {StrL("![alt](a.png)"), {}, &para};
    SrcDoc d;
    d.Run("see ", &plain, false);
    d.Image(&img, true);
    d.Run(" now", &plain, true);
    utassert(SrcIs(SrcCopy(&d, SelectionFormat::Source, buf, sizeof(buf)),
                   "see ![alt](a.png) now"));
    utassert(SrcIs(SrcCopy(&d, SelectionFormat::Plain, buf, sizeof(buf)),
                   "see  now"));
    // Stopping at the end of the run before it still reaches it: the run
    // after has nothing selected in it, which is the trailing case.
    int n = CopyTextHitsIn(&d.ctx, 0, 4, -1, buf, sizeof(buf),
                           SelectionFormat::Source);
    utassert(SrcIs(Str(buf, n), "see ![alt](a.png)"));
    // Stopping short of that end does not.
    n = CopyTextHitsIn(&d.ctx, 0, 2, -1, buf, sizeof(buf),
                       SelectionFormat::Source);
    utassert(SrcIs(Str(buf, n), "se"));
    // Nor does a selection that starts after the picture.
    n = CopyTextHitsIn(&d.ctx, 6, 10, -1, buf, sizeof(buf),
                       SelectionFormat::Source);
    utassert(SrcIs(Str(buf, n), " now"));
}

// The two ends of the same rule: a paragraph that begins or ends with an
// image has no run on that side, and that counts as reaching it.
static void TestSourceImageAtTheEnds() {
    char buf[512];
    SelBlock para = {};
    SelSource plain = {{}, {}, &para};
    SelSource img = {StrL("![alt](a.png)"), {}, &para};
    // Leading: selecting the words after the picture takes the picture.
    SrcDoc lead;
    lead.Image(&img, false);
    lead.Run(" now", &plain, true);
    int n = CopyTextHitsIn(&lead.ctx, 1, 5, -1, buf, sizeof(buf),
                           SelectionFormat::Source);
    utassert(SrcIs(Str(buf, n), "![alt](a.png) now"));
    // Trailing: selecting the words before it does too.
    SrcDoc tail;
    tail.Run("see ", &plain, false);
    tail.Image(&img, true);
    n = CopyTextHitsIn(&tail.ctx, 0, 4, -1, buf, sizeof(buf),
                       SelectionFormat::Source);
    utassert(SrcIs(Str(buf, n), "see ![alt](a.png)"));
    // A picture with no words either side is the whole paragraph, and Rust
    // emits nothing for such a paragraph: it is the document walk that takes
    // it when what encloses it is selected. Here that is the selection having
    // run past the place it sits in.
    SelBlock next = {};
    SelSource below = {{}, {}, &next};
    SrcDoc lone;
    lone.Image(&img, false);
    lone.Run("after", &below, false);
    utassert(SrcIs(SrcCopy(&lone, SelectionFormat::Source, buf,
                          sizeof(buf)),
                   "![alt](a.png)\nafter"));
    // The paragraph below it on its own leaves it behind.
    n = CopyTextHitsIn(&lone.ctx, 1, 6, -1, buf, sizeof(buf),
                       SelectionFormat::Source);
    utassert(SrcIs(Str(buf, n), "after"));
}

// A run that names no source — everything outside a TextView — copies as its
// own text in both formats, one run per line, which is what the copier did
// before there was a second format at all.
static void TestSourceIgnoresPlainRuns() {
    char buf[512];
    SrcDoc d;
    d.Run("hello", nullptr, false);
    d.Run("world", nullptr, false);
    utassert(SrcIs(SrcCopy(&d, SelectionFormat::Source, buf, sizeof(buf)),
                   "hello\nworld"));
    utassert(SrcIs(SrcCopy(&d, SelectionFormat::Plain, buf, sizeof(buf)),
                   "hello\nworld"));
}

void TestTextView() {
    TestSuite("TextView");
    Arena* a = ArenaNew();
    TestMarkdownBlocks(a);
    TestMarkdownTableAlign(a);
    TestMarkdownInlineHtml(a);
    TestMarkdownHtmlBlock(a);
    TestMarkdownImage(a);
    TestHtmlBlocks(a);
    TestHtmlInlineMarks(a);
    TestHtmlNestedMarks(a);
    TestHtmlWhitespace(a);
    TestHtmlEntities(a);
    TestHtmlList(a);
    TestHtmlPre(a);
    TestHtmlTable(a);
    TestHtmlImageAlt(a);
    TestHtmlUnbalanced(a);
    TestHtmlComments(a);
    TestHtmlBreak(a);
    TestHtmlImage(a);
    TestImageSrc();
    TestSourceMarks();
    TestSourcePartialMark();
    TestSourceCodeAndLink();
    TestSourceHeading();
    TestSourceBlockquote();
    TestSourceCodeBlock();
    TestSourceTable();
    TestSourceList();
    TestSourceTaskList();
    TestSourceImage();
    TestSourceImageAtTheEnds();
    TestSourceIgnoresPlainRuns();
    TestMarkdownTaskList(a);
    ArenaDelete(a);
}
