/* Tests for src/markdown, the C++ port of the `markdown` crate 1.0.0.

   The crate's own suite is split in two. Its `#[cfg(test)]` modules pin the
   constants, the character classifier and the option shortcuts, and those are
   ported below. Everything else it has lives in the crate's `tests/`
   directory — around 8000 CommonMark and GFM cases — which is not part of the
   published crate, the same gap `port-upstream.md` describes for taffy. What
   stands in for them here is the second half of this file: an end-to-end
   check of each construct, reading the mdast the way TextView does.

   Not ported, and named in port-progress.md: the `Debug`/`serde` cases in
   `mdast.rs`, `unist.rs` and `configuration.rs` (neither trait is ported),
   and everything in `util/mdx.rs` and `util/location.rs` (MDX is not
   ported). */

#include "Test.h"

using namespace markdown;

// The tree of a source, in an arena the caller owns.
static Node* Parse(Arena* a, const char* source) {
    return ToMdast(a, Str(source), ParseOptions::Gfm());
}

// The n-th child, or null.
static Node* Child(Node* n, int32_t ix) {
    if (!n || ix >= n->children.len) {
        return nullptr;
    }
    return n->children[ix];
}

static bool Is(Str s, const char* want) {
    int32_t len = (int32_t)strlen(want);
    return s.len == len && (len == 0 || memcmp(s.s, want, (size_t)len) == 0);
}

// The text of a node and everything under it.
static bool TextIs(Arena* a, Node* n, const char* want) {
    return n && Is(NodeToString(a, n), want);
}

// ─── src/util/constant.rs: `constants` ────────────────────────────────────

static void TestMarkdownConstants() {
    TestSuite("markdown constants");
    // The digits of the largest code point, in each base: "1114111" and
    // "10ffff".
    utassert(kCharacterReferenceDecimalSizeMax == fmt("%d", 0x10ffff).len);
    utassert(kCharacterReferenceHexadecimalSizeMax == fmt("%x", 0x10ffff).len);

    // The two runs and the table are three parallel things now, so the walk
    // has to agree with the offsets the table holds: an entry pointing a byte
    // into the middle of a name would still read as a string, just the wrong
    // one, and nothing else would notice.
    utassert(base::SeqStrCount(kCharacterReferenceNames) == 2125);
    utassert(base::SeqStrCount(kCharacterReferenceValues) == 2125);
    int nameOff = 0;
    int valueOff = 0;
    int32_t longestName = 0;
    for (int32_t i = 0; i < 2125; i++) {
        utassert(kCharacterReferences[i].nameOff == nameOff);
        utassert(kCharacterReferences[i].valueOff == valueOff);
        Str name = base::SeqStrAt(kCharacterReferenceNames, nameOff);
        utassert(name.len > 0);
        if (name.len > longestName) {
            longestName = name.len;
        }
        base::SeqStrAdvance(kCharacterReferenceNames, nameOff);
        base::SeqStrAdvance(kCharacterReferenceValues, valueOff);
    }
    utassert(kCharacterReferenceNamedSizeMax == longestName);

    int32_t longestRaw = 0;
    int rawOff = 0;
    int rawCount = 0;
    while (kHtmlRawNames[rawOff]) {
        Str raw = base::SeqStrAt(kHtmlRawNames, rawOff);
        if (raw.len > longestRaw) {
            longestRaw = raw.len;
        }
        rawCount++;
        if (!base::SeqStrAdvance(kHtmlRawNames, rawOff)) {
            break;
        }
    }
    utassert(rawCount == 4);
    utassert(kHtmlRawSizeMax == longestRaw);
    utassert(base::SeqStrCount(kHtmlBlockNames) == 62);

    // The names are sorted, which is what `DecodeNamed` binary searches.
    for (int32_t i = 1; i < 2125; i++) {
        const char* prev =
            kCharacterReferenceNames + kCharacterReferences[i - 1].nameOff;
        const char* cur =
            kCharacterReferenceNames + kCharacterReferences[i].nameOff;
        utassert(strcmp(prev, cur) < 0);
    }
}

// ─── src/util/char.rs: `test_classify` ────────────────────────────────────

// `gpui::CharKind` is a different enum, so this one says which it means.
static void TestMarkdownClassify() {
    TestSuite("markdown classify");
    utassert(Classify(' ') == markdown::CharKind::Whitespace);
    utassert(Classify('.') == markdown::CharKind::Punctuation);
    utassert(Classify('a') == markdown::CharKind::Other);
    // Beyond ASCII, which is what util/unicode.rs is for.
    utassert(Classify(0x00a0) == markdown::CharKind::Whitespace); // no-break space
    utassert(Classify(0x2014) == markdown::CharKind::Punctuation); // em dash
    utassert(Classify(0x00e9) == markdown::CharKind::Other);       // é
    // End of file counts as whitespace.
    utassert(Classify(-1) == markdown::CharKind::Whitespace);
}

// ─── src/configuration.rs: `test_constructs` / `test_parse_options` ───────

static void TestMarkdownOptions() {
    TestSuite("markdown options");
    Constructs constructs;
    utassert(constructs.attention);
    utassert(!constructs.gfmAutolinkLiteral);
    utassert(!constructs.frontmatter);

    constructs = Constructs::Gfm();
    utassert(constructs.attention);
    utassert(constructs.gfmAutolinkLiteral);
    utassert(!constructs.frontmatter);

    ParseOptions options;
    utassert(options.constructs.attention);
    utassert(!options.constructs.gfmAutolinkLiteral);
    utassert(options.gfmStrikethroughSingleTilde);

    options = ParseOptions::Gfm();
    utassert(options.constructs.gfmAutolinkLiteral);
}

// ─── src/util/character_reference.rs ──────────────────────────────────────

static void TestMarkdownCharacterReference(Arena* a) {
    TestSuite("markdown character reference");
    utassert(Is(DecodeNamed(a, StrL("amp")), "&"));
    utassert(Is(DecodeNamed(a, StrL("AMP")), "&"));
    utassert(Is(DecodeNamed(a, StrL("copy")), "\xc2\xa9"));
    utassert(Is(DecodeNamed(a, StrL("CounterClockwiseContourIntegral")),
                "\xe2\x88\xb3"));
    // The two values `constant.cpp` writes as `\u` escapes rather than as
    // themselves, because gcc rejects a bidi character in a literal.
    utassert(Is(DecodeNamed(a, StrL("lrm")), "\xe2\x80\x8e"));
    utassert(Is(DecodeNamed(a, StrL("rlm")), "\xe2\x80\x8f"));
    // Not a name: `None`.
    utassert(DecodeNamed(a, StrL("nope")).s == nullptr);
    utassert(DecodeNamed(a, StrL("")).s == nullptr);

    utassert(Is(DecodeNumeric(a, StrL("65"), 10), "A"));
    utassert(Is(DecodeNumeric(a, StrL("41"), 16), "A"));
    // Out of range, a surrogate, and a forbidden control: U+FFFD.
    utassert(Is(DecodeNumeric(a, StrL("1114112"), 10), "\xef\xbf\xbd"));
    utassert(Is(DecodeNumeric(a, StrL("d800"), 16), "\xef\xbf\xbd"));
    utassert(Is(DecodeNumeric(a, StrL("0"), 10), "\xef\xbf\xbd"));
}

// ─── src/util/normalize_identifier.rs ─────────────────────────────────────

static void TestMarkdownNormalizeIdentifier(Arena* a) {
    TestSuite("markdown normalize identifier");
    utassert(Is(NormalizeIdentifier(a, StrL(" a ")), "A"));
    utassert(Is(NormalizeIdentifier(a, StrL(" a\n b")), "A B"));
    utassert(Is(NormalizeIdentifier(a, StrL("")), ""));
    // The crate's own quirk, kept: whitespace after a word that starts at
    // offset 0 collapses to nothing rather than to a space, because `start`
    // is what it tests for "have we written anything yet". Its doc comment
    // says `a\t\r\nb` normalizes to `a b`; markdown-rs 1.0.0 answers `ab`,
    // and so does this, so a reference here matches the definitions it does.
    utassert(Is(NormalizeIdentifier(a, StrL("a\n b")), "AB"));
    utassert(Is(NormalizeIdentifier(a, StrL("Foo\t\tBar")), "FOOBAR"));
}

// ─── the tree ─────────────────────────────────────────────────────────────

static void TestMarkdownFlow(Arena* a) {
    TestSuite("markdown flow");
    Node* root = Parse(a, "# Hi *Earth*!\n");
    utassert(root->kind == NodeKind::Root);
    utassert(root->children.len == 1);
    Node* heading = Child(root, 0);
    utassert(heading->kind == NodeKind::Heading);
    utassert(heading->depth == 1);
    utassert(TextIs(a, heading, "Hi Earth!"));
    utassert(Child(heading, 1)->kind == NodeKind::Emphasis);
    // Positions are byte offsets into the source.
    utassert(heading->position.start.offset == 0);
    utassert(heading->position.end.offset == 13);

    root = Parse(a, "Setext\n===\n");
    utassert(Child(root, 0)->kind == NodeKind::Heading);
    utassert(Child(root, 0)->depth == 1);

    root = Parse(a, "a\n\n> b\n>\n> c\n");
    utassert(root->children.len == 2);
    utassert(Child(root, 0)->kind == NodeKind::Paragraph);
    utassert(Child(root, 1)->kind == NodeKind::Blockquote);
    utassert(Child(root, 1)->children.len == 2);

    root = Parse(a, "***\n");
    utassert(Child(root, 0)->kind == NodeKind::ThematicBreak);

    root = Parse(a, "```rust meta\nlet a = 1;\n```\n");
    Node* code = Child(root, 0);
    utassert(code->kind == NodeKind::Code);
    utassert(Is(code->lang, "rust"));
    utassert(Is(code->meta, "meta"));
    utassert(Is(code->value, "let a = 1;"));

    root = Parse(a, "    indented\n");
    utassert(Child(root, 0)->kind == NodeKind::Code);
    utassert(Is(Child(root, 0)->value, "indented"));
    utassert(Child(root, 0)->lang.s == nullptr);
}

static void TestMarkdownLists(Arena* a) {
    TestSuite("markdown lists");
    Node* root = Parse(a, "- one\n- two\n");
    Node* list = Child(root, 0);
    utassert(list->kind == NodeKind::List);
    utassert(!list->ordered);
    utassert(!list->spread);
    utassert(list->children.len == 2);
    utassert(TextIs(a, Child(list, 1), "two"));

    root = Parse(a, "3. three\n4. four\n");
    list = Child(root, 0);
    utassert(list->ordered);
    utassert(list->hasStart);
    utassert(list->start == 3);

    // A blank line between items makes the list loose.
    root = Parse(a, "- one\n\n- two\n");
    utassert(Child(root, 0)->spread);

    // GFM task lists.
    root = Parse(a, "- [x] done\n- [ ] todo\n");
    list = Child(root, 0);
    utassert(Child(list, 0)->hasChecked);
    utassert(Child(list, 0)->checked);
    utassert(Child(list, 1)->hasChecked);
    utassert(!Child(list, 1)->checked);
    // The checkbox is not part of the text.
    utassert(TextIs(a, Child(list, 0), "done"));
}

static void TestMarkdownText(Arena* a) {
    TestSuite("markdown text");
    Node* root = Parse(a, "a **b** _c_ `d` ~~e~~\n");
    Node* p = Child(root, 0);
    utassert(p->kind == NodeKind::Paragraph);
    utassert(Child(p, 1)->kind == NodeKind::Strong);
    utassert(Child(p, 3)->kind == NodeKind::Emphasis);
    utassert(Child(p, 5)->kind == NodeKind::InlineCode);
    utassert(Is(Child(p, 5)->value, "d"));
    utassert(Child(p, 7)->kind == NodeKind::Delete);

    // A character reference decodes; an escape keeps the character.
    root = Parse(a, "&amp; &#65; \\*not em\\*\n");
    utassert(TextIs(a, Child(root, 0), "& A *not em*"));

    // Two trailing spaces are a hard break.
    root = Parse(a, "a  \nb\n");
    p = Child(root, 0);
    utassert(Child(p, 1)->kind == NodeKind::Break);

    // A backslash at the end of a line is one too.
    root = Parse(a, "a\\\nb\n");
    utassert(Child(Child(root, 0), 1)->kind == NodeKind::Break);
}

static void TestMarkdownLinks(Arena* a) {
    TestSuite("markdown links");
    Node* root = Parse(a, "[text](/url \"title\")\n");
    Node* link = Child(Child(root, 0), 0);
    utassert(link->kind == NodeKind::Link);
    utassert(Is(link->url, "/url"));
    utassert(Is(link->title, "title"));
    utassert(TextIs(a, link, "text"));

    root = Parse(a, "![alt](/img.png)\n");
    Node* image = Child(Child(root, 0), 0);
    utassert(image->kind == NodeKind::Image);
    utassert(Is(image->url, "/img.png"));
    utassert(Is(image->alt, "alt"));

    // A definition and the three kinds of reference to it.
    root = Parse(a, "[Foo]: /f\n\n[Foo]\n\n[bar][Foo]\n\n[Foo][]\n");
    Node* definition = Child(root, 0);
    utassert(definition->kind == NodeKind::Definition);
    utassert(Is(definition->identifier, "foo"));
    utassert(Is(definition->url, "/f"));
    Node* shortcut = Child(Child(root, 1), 0);
    utassert(shortcut->kind == NodeKind::LinkReference);
    utassert(shortcut->referenceKind == ReferenceKind::Shortcut);
    utassert(Is(shortcut->identifier, "foo"));
    utassert(Child(Child(root, 2), 0)->referenceKind == ReferenceKind::Full);
    utassert(Child(Child(root, 3), 0)->referenceKind ==
             ReferenceKind::Collapsed);

    // An undefined reference is not a link at all.
    root = Parse(a, "[nope]\n");
    utassert(Child(Child(root, 0), 0)->kind == NodeKind::Text);

    // Autolinks, and the GFM literal kind.
    root = Parse(a, "<https://x.com/> and www.y.com and a@b.com\n");
    Node* p = Child(root, 0);
    utassert(Child(p, 0)->kind == NodeKind::Link);
    utassert(Is(Child(p, 0)->url, "https://x.com/"));
    utassert(Is(Child(p, 2)->url, "http://www.y.com"));
    utassert(Is(Child(p, 4)->url, "mailto:a@b.com"));
}

static void TestMarkdownTable(Arena* a) {
    TestSuite("markdown table");
    Node* root = Parse(a, "| a | b | c | d |\n"
                          "| - |:- |:-:| -:|\n"
                          "| 1 | 2 | 3 | 4 |\n");
    Node* table = Child(root, 0);
    utassert(table->kind == NodeKind::Table);
    utassert(table->align.len == 4);
    utassert(table->align[0] == AlignKind::None);
    utassert(table->align[1] == AlignKind::Left);
    utassert(table->align[2] == AlignKind::Center);
    utassert(table->align[3] == AlignKind::Right);
    utassert(table->children.len == 2);
    Node* head = Child(table, 0);
    utassert(head->kind == NodeKind::TableRow);
    utassert(head->children.len == 4);
    utassert(Child(head, 0)->kind == NodeKind::TableCell);
    utassert(TextIs(a, Child(head, 2), "c"));
    utassert(TextIs(a, Child(Child(table, 1), 3), "4"));
}

static void TestMarkdownHtmlAndFootnotes(Arena* a) {
    TestSuite("markdown html");
    Node* root = Parse(a, "<div>\n  <b>x</b>\n</div>\n");
    utassert(Child(root, 0)->kind == NodeKind::Html);
    utassert(Is(Child(root, 0)->value, "<div>\n  <b>x</b>\n</div>"));

    root = Parse(a, "a <b>c</b> d\n");
    Node* p = Child(root, 0);
    utassert(Child(p, 1)->kind == NodeKind::Html);
    utassert(Is(Child(p, 1)->value, "<b>"));

    TestSuite("markdown footnotes");
    root = Parse(a, "Call[^1].\n\n[^1]: The note.\n");
    Node* call = Child(Child(root, 0), 1);
    utassert(call->kind == NodeKind::FootnoteReference);
    utassert(Is(call->identifier, "1"));
    Node* definition = Child(root, 1);
    utassert(definition->kind == NodeKind::FootnoteDefinition);
    utassert(Is(definition->identifier, "1"));
    utassert(TextIs(a, definition, "The note."));
}

// A document with one of everything, which is what the story's README looks
// like: it is here to catch a crash rather than to pin a shape.
static void TestMarkdownDocument(Arena* a) {
    TestSuite("markdown document");
    Node* root = Parse(a,
                       "# Title\n"
                       "\n"
                       "Text with **bold**, a [link](/l) and `code`.\n"
                       "\n"
                       "> A quote\n"
                       "> over two lines.\n"
                       "\n"
                       "1. first\n"
                       "2. second\n"
                       "   - nested\n"
                       "\n"
                       "| a | b |\n"
                       "| - | - |\n"
                       "| 1 | 2 |\n"
                       "\n"
                       "```c\n"
                       "int main(void) { return 0; }\n"
                       "```\n"
                       "\n"
                       "---\n"
                       "\n"
                       "![img](/i.png)\n");
    utassert(root->children.len == 8);
    utassert(Child(root, 0)->kind == NodeKind::Heading);
    utassert(Child(root, 1)->kind == NodeKind::Paragraph);
    utassert(Child(root, 2)->kind == NodeKind::Blockquote);
    utassert(Child(root, 3)->kind == NodeKind::List);
    utassert(Child(root, 4)->kind == NodeKind::Table);
    utassert(Child(root, 5)->kind == NodeKind::Code);
    utassert(Child(root, 6)->kind == NodeKind::ThematicBreak);
    utassert(Child(root, 7)->kind == NodeKind::Paragraph);

    // The empty document is a root with nothing in it.
    root = ToMdast(a, Str("", 0), ParseOptions::Gfm());
    utassert(root->kind == NodeKind::Root);
    utassert(root->children.len == 0);
}

void TestMarkdown() {
    Arena* a = ArenaNew();
    TestMarkdownConstants();
    TestMarkdownClassify();
    TestMarkdownOptions();
    TestMarkdownCharacterReference(a);
    TestMarkdownNormalizeIdentifier(a);
    TestMarkdownFlow(a);
    TestMarkdownLists(a);
    TestMarkdownText(a);
    TestMarkdownLinks(a);
    TestMarkdownTable(a);
    TestMarkdownHtmlAndFootnotes(a);
    TestMarkdownDocument(a);
    ArenaDelete(a);
}
