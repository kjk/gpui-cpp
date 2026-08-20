/* Themed markdown view — crates/ui/src/text.

   Rust parses with the `markdown` crate into an mdast, folds that into its own
   BlockNode tree (text/node.rs) and renders the tree. This is the same shape:
   md4c (ext/md4c) parses SAX-style, its callbacks build the MdNode tree below,
   and TextView::IntoEl walks it. md4c runs in the GitHub dialect, so tables,
   strikethrough, task lists and bare-URL autolinks all work.

   Raw HTML is the other half. Rust parses it with html5ever and folds the
   DOM into the same BlockNode tree (text/format/html.rs); here ui/html.cpp is
   a small tokenizer that folds tags into the same MdNode tree, so an HTML
   block inside markdown, an inline <b> or <a>, and a whole HTML document all
   render through the one walk below.

   This is the one rich-text renderer in the tree. Rust has exactly one too —
   everything that shows markdown or HTML goes through TextView — so a page
   that needs it should come here rather than grow another parser.

   A fenced code block is highlighted by ui/syntax.h, a scanner rather than
   the tree-sitter parse Rust runs: it answers the capture names position and
   a keyword list can settle and paints them from the same theme table.

   An image — ![alt](src) or <img> — is a run of its own in the flow, drawn
   by gpui/image.h from the asset roots or from a data: URI. What it cannot
   reach it shows as its alt text, and that is most of what a document
   written for the web holds: fetching an http(s) URL needs a socket and a
   TLS stack this tree does not have.

   Where it stops short of Rust: selection is still per element
   (base/text_selection.cpp), not the window-wide one
   text/window_selection.rs runs. */

#include "ui/sizing.h"
#include "ui/syntax.h"

namespace gpui {

namespace component {

// text/node.rs TextMark.
enum MdMark : uint8_t {
    MdBold = 1 << 0,
    MdItalic = 1 << 1,
    MdCode = 1 << 2,
    MdDel = 1 << 3,
    MdUnderline = 1 << 4,
    MdLink = 1 << 5,
    // <mark>: TextMark::highlight, painted with the theme's yellow behind it.
    // Rust reads a color off the tag; this takes the default one.
    MdHighlight = 1 << 6,
};

// One styled piece of a paragraph: node.rs's (text, TextMark) pair, or —
// when `imgSrc` is set — node.rs's InlineNode::image, an ImageNode sitting in
// the flow with the words. `text` is then the alt text, which is what paints
// if the source will not decode (gpui/image.h says when that is).
struct MdRun {
    Str text = {};
    // LinkMark::url, when marks has MdLink.
    Str href = {};
    // ImageNode::url. An image run carries no other text.
    Str imgSrc = {};
    // ImageNode::width / height, when the document gave them. 0 is "its own".
    float imgW = 0;
    float imgH = 0;
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
    // A raw HTML block inside markdown. Its children are what ui/html.cpp
    // made of the raw text; Rust reaches the same place through
    // markdown_ext.rs handing the html node to format::html.
    Html,
    // An HTML container that is not a block of its own — div, section, li's
    // wrapper, <figure>. BlockNode::Root in Rust: it contributes its
    // children and no box.
    Group,
};

// MD_ALIGN, repeated so ui/html.cpp does not have to include md4c.h.
enum MdAlign : uint8_t {
    MdAlignDefault = 0,
    MdAlignLeft = 1,
    MdAlignCenter = 2,
    MdAlignRight = 3,
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
    // Whether `source` is HTML rather than markdown — TextView::html().
    bool html = false;
    // text_view.rs link_click_handler.
    Listener onLink;
    // node.rs min_w_16: the floor a table column shrinks to. Above the floor
    // a column's width is a fraction of the table, proportional to the length
    // of its content, the way render_wrap_table distributes the space.
    float tableColW = 64;

    // text_view.rs TextView::markdown / TextView::html.
    static TextView* New(Ctx* cx, Str source);
    static TextView* NewHtml(Ctx* cx, Str source);
    TextView* Font(float px);
    TextView* HeadingFont(float px);
    TextView* Selectable(bool on = true);
    TextView* TableColumnWidth(float px);
    TextView* ParagraphGap(float px);
    // text_view::LinkClickHandlerFn. The handler's intptr_t is the link's
    // href as a NUL-terminated `const char*`; it points into the parse the
    // frame was built from and is good for the length of the call, which is
    // the same rule every other hit-test payload follows. Without a handler
    // a link opens in the desktop's browser, which is what Rust's
    // handle_link_click falls back to (cx.open_url).
    TextView* OnLink(Listener fn);
    El* IntoEl();

  private:
    // node.rs render_block. `depth` is the list nesting level, `inList` and
    // `isLast` decide whether the block carries a paragraph gap below it.
    El* Block(MdNode* n, int depth, bool inList, bool isLast);
    El* Blocks(El* into, MdNode* n, int depth, bool inList);
    El* Item(MdNode* n, Str marker, int depth);
    El* Table(MdNode* n);
    El* CodeBlock(MdNode* n);
    // The highlighted form of a code block: ui/syntax.h scans the text and
    // this paints its tokens.
    El* CodeLines(Str code, SyntaxLang lang);
    // An image run: node.rs putting an img() element in the middle of the
    // inline flow.
    El* ImageRun(MdRun* r, float font, Rgba color);
    // One styled word of a flow, with its marks applied and — for a link —
    // the click that opens it.
    El* Word(Str w, float font, Rgba color, uint8_t marks, int weight,
             Str href);
    // The inline flow of a block, as a column of wrapping rows — a hard break
    // starts a new row. `weight` is 0 normal, 1 medium, 2 semibold, 3 bold.
    El* Inline(MdNode* n, float font, Rgba color, int weight,
               uint8_t align = MdAlignDefault);
};

// Parses `source` into a block tree allocated from `a`. Exposed for tests.
MdNode* MdParse(Arena* a, Str source);

// "&amp;" -> "&", for the entities that show up in prose. Returns the text
// unchanged when it is not an entity we know. Shared with ui/html.cpp.
Str MdDecodeEntity(Arena* a, Str e);

} // namespace component
} // namespace gpui
