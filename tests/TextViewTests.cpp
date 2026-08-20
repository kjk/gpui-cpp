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
    utassert(TextIs(a, Child(list, 1), "two"));
}

// The delimiter row's colons, which node.rs render_wrap_table aligns each
// column by. md4c reports them as MD_ALIGN on every cell of the column.
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

// Inline HTML inside a paragraph: md4c hands the tags over as raw text and
// text.cpp turns them into the marks html5ever would have produced.
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

void TestTextView() {
    TestSuite("TextView");
    Arena* a = ArenaNew();
    TestMarkdownBlocks(a);
    TestMarkdownTableAlign(a);
    TestMarkdownInlineHtml(a);
    TestMarkdownHtmlBlock(a);
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
    ArenaDelete(a);
}
