/* Themed markdown view — crates/ui/src/text.

   Rust parses with the `markdown` crate into an mdast, folds that into its own
   BlockNode tree (text/node.rs) and renders the tree. This is the same shape:
   md4c (ext/md4c) parses SAX-style, its callbacks build the MdNode tree below,
   and TextView::IntoEl walks it. md4c runs in the GitHub dialect, so tables,
   strikethrough, task lists and bare-URL autolinks all work.

   This is the one markdown renderer in the tree. Rust has exactly one too —
   everything that shows markdown goes through TextView — so a page that needs
   it should come here rather than grow another parser.

   Where it stops short of Rust: no syntax highlighting inside code blocks (no
   tree-sitter here), no images, no link click handler, no strikethrough and
   no table column alignment. */

#include "component/Common.h"

namespace gpui {

namespace component {

// text/node.rs TextMark. The paint layer has no strikethrough, so `del` is
// drawn in muted_foreground instead of struck through.
enum MdMark : uint8_t {
    MdBold = 1 << 0,
    MdItalic = 1 << 1,
    MdCode = 1 << 2,
    MdDel = 1 << 3,
    MdUnderline = 1 << 4,
    MdLink = 1 << 5,
};

// One styled piece of a paragraph: node.rs's (text, TextMark) pair.
struct MdRun {
    Str text = {};
    // LinkMark::url, when marks has MdLink.
    Str href = {};
    MdRun* next = nullptr;
    uint8_t marks = 0;
};

// text/node.rs BlockNode. Table rows and cells are blocks here rather than
// the separate TableRow / TableCell structs Rust uses; the tree walk is the
// same either way.
enum class MdKind : uint8_t {
    Doc,
    Paragraph,
    Heading,
    Quote,
    List,
    Item,
    Code,
    Table,
    Row,
    Cell,
    Rule,
    // A raw HTML block. Rust folds these to BlockNode::Unknown and renders an
    // empty div; so does this.
    Html,
};

struct MdNode {
    MdKind kind = MdKind::Doc;
    MdNode* parent = nullptr;
    MdNode* first = nullptr;
    MdNode* last = nullptr;
    MdNode* next = nullptr;
    // Inline content, for Paragraph, Heading, Cell and Code.
    MdRun* runFirst = nullptr;
    MdRun* runLast = nullptr;
    // Code: the fence's info string, e.g. "cpp".
    Str lang = {};
    // List: the first ordered number.
    int start = 1;
    // Heading: 1..6.
    uint8_t level = 0;
    // Cell: MD_ALIGN.
    uint8_t align = 0;
    bool ordered = false;
    // Row: this row is the table head.
    bool head = false;
};

struct TextView {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str source = {};
    // Body text size. In Rust this is whatever the TextView inherits, which
    // is theme.font_size — 16 — and is separate from the heading base below.
    float baseFont = 16;
    // TextViewStyle::heading_base_font_size. Heading sizes are multiples of
    // it: node.rs 2258 has h1 2.0, h2 1.5, h3 1.25, h4 1.125, h5 and h6 1.0.
    float headingFont = 14;
    // theme.mono_font_size — code blocks and inline code.
    float codeFont = 13;
    // TextViewStyle::paragraph_gap, rems(1.).
    float paragraphGap = 16;
    // Whether the text can be dragged over. Rust's TextView is selectable
    // through its own selection machinery; here it is El::Selectable.
    bool selectable = false;
    // node.rs min_w_16: the floor a table column shrinks to. Above the floor
    // a column's width is a fraction of the table, proportional to the length
    // of its content, the way render_wrap_table distributes the space.
    float tableColW = 64;

    static TextView* New(Ctx* cx, Str source);
    TextView* Font(float px);
    TextView* HeadingFont(float px);
    TextView* Selectable(bool on = true);
    TextView* TableColumnWidth(float px);
    TextView* ParagraphGap(float px);
    El* IntoEl();

  private:
    // node.rs render_block. `depth` is the list nesting level, `inList` and
    // `isLast` decide whether the block carries a paragraph gap below it.
    El* Block(MdNode* n, int depth, bool inList, bool isLast);
    El* Blocks(El* into, MdNode* n, int depth, bool inList);
    El* Item(MdNode* n, Str marker, int depth);
    El* Table(MdNode* n);
    El* CodeBlock(MdNode* n);
    // The inline flow of a block, as a column of wrapping rows — a hard break
    // starts a new row. `weight` is 0 normal, 1 medium, 2 semibold, 3 bold.
    El* Inline(MdNode* n, float font, Rgba color, int weight);
};

// Parses `source` into a block tree allocated from `a`. Exposed for tests.
MdNode* MdParse(Arena* a, Str source);

} // namespace component
} // namespace gpui
