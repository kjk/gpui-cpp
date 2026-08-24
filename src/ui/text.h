/* Themed markdown view — crates/ui/src/text.

   Rust parses with the `markdown` crate into an mdast, folds that into its own
   BlockNode tree (text/node.rs) and renders the tree. This is the same shape
   and the same parser: src/markdown is that crate ported (see
   src/markdown/readme.md), text.cpp folds the mdast it returns into the MdNode
   tree below, and TextView::IntoEl walks it. It runs in the GFM dialect, the
   one markdown_ext.rs asks for, so tables, strikethrough, task lists,
   footnotes and bare-URL autolinks all work.

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

   Selection is the window's: say Selectable() on the text and a drag runs
   from one paragraph into the next, across every element between them
   (base/text_selection.h WindowSelection). */

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

// mdast's AlignKind, repeated so ui/html.cpp — which has no mdast of its own
// — can say the same thing.
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
    // Html: the block's raw source, kept after MdExpandHtml has turned it
    // into children — a plugin matches on the tag it names, which is what
    // Rust's `Node::Html(raw)` arm reads.
    Str raw = {};
    // List: the first ordered number.
    int start = 1;
    // Heading: 1..6.
    uint8_t level = 0;
    // Cell: MD_ALIGN.
    uint8_t align = 0;
    bool ordered = false;
    // Row: this row is the table head.
    bool head = false;
    // Item: the GFM task list checkbox — whether the item carries one at all,
    // and whether it is ticked. BlockNode::ListItem's `Option<bool> checked`,
    // as the pair mdast keeps it as. ui/html.cpp leaves both unset, which is
    // what format/html.rs does.
    bool hasCheck = false;
    bool checked = false;
};

// text_view.rs MarkdownNode: one block a plugin claimed, with the payload its
// parser made and the two strings a copy of it would carry.
struct MdPluginNode {
    // MarkdownNode::name — the plugin that owns it.
    Str name = {};
    // as_text / as_markdown: what the block reads as, and the markdown that
    // would produce it again.
    Str text = {};
    Str markdown = {};
    // The parser's own payload, on the frame arena the parse ran in.
    void* data = nullptr;
};

// MarkdownPlugin::parse. `node` is the block, `text` its flattened text —
// the raw source for an HTML block, which is what a tag plugin matches on.
// True when the plugin claims the block.
using MdPluginParseFn = bool (*)(Ctx* cx, MdNode* node, Str text, void* data,
                                 MdPluginNode* out);
// MarkdownPlugin::render, for a block its own parser claimed.
using MdPluginRenderFn = El* (*)(Ctx * cx, const MdPluginNode* node,
                                 void* data);

// One registered extension: `markdown(..).plugin(TickerPlugin::new(..))`.
struct MdPlugin {
    Str name = {};
    MdPluginParseFn parse = nullptr;
    MdPluginRenderFn render = nullptr;
    void* data = nullptr;
};

// text_view.rs CodeBlockActionsFn: what the caller hangs in the corner of a
// fenced block. Upstream's markdown example puts a Clipboard there and a Run
// button on the languages it knows. `code` is the block's text and `lang` its
// info string, both good for the length of the call.
using CodeBlockActionsFn = El* (*)(Ctx * cx, void* data, Str code, Str lang);

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
    CodeBlockActionsFn codeActions = nullptr;
    void* codeActionsData = nullptr;
    // As many as a view registers; upstream's own example has three.
    static const int kMaxPlugins = 8;
    MdPlugin plugins[kMaxPlugins] = {};
    int nPlugins = 0;
    // node.rs min_w_16: the floor a table column shrinks to. Above the floor
    // a column's width is a fraction of the table, proportional to the length
    // of its content, the way render_wrap_table distributes the space.
    float tableColW = 64;
    // TextViewStyle::table with overflow-x: scroll. A table laid out this way
    // takes its column widths from the measured text rather than from a
    // character count, and scrolls sideways once the columns are down to
    // their floors — node.rs render_scroll_table, which is what the markdown
    // example defaults to.
    bool tableScroll = false;
    // How many scrolling tables have been built this frame, which is what
    // names each one's scroll offset.
    int tableIx = 0;
    // TextView::selection_format. Rust keeps it on the view's own state and
    // the document reconstructs the source when the copy asks for it; the
    // selection here is the window's, so the view pushes the format onto it
    // and every run carries the Markdown around it (gpui::SelSource).
    gpui::SelectionFormat selFormat = gpui::SelectionFormat::Plain;

    // text_view.rs TextView::markdown / TextView::html.
    static TextView* New(Ctx* cx, Str source);
    static TextView* NewHtml(Ctx* cx, Str source);
    TextView* Font(float px);
    TextView* HeadingFont(float px);
    TextView* Selectable(bool on = true);
    // `.selection_format(..)`: whether a copy of the selection is the text as
    // rendered or the Markdown it was rendered from. Only meaningful on a
    // Selectable() view.
    TextView* SelFormat(gpui::SelectionFormat fmt);
    TextView* TableColumnWidth(float px);
    TextView* TableScroll(bool on = true);
    TextView* ParagraphGap(float px);
    // text_view::LinkClickHandlerFn. The handler's intptr_t is the link's
    // href as a NUL-terminated `const char*`; it points into the parse the
    // frame was built from and is good for the length of the call, which is
    // the same rule every other hit-test payload follows. Without a handler
    // a link opens in the desktop's browser, which is what Rust's
    // handle_link_click falls back to (cx.open_url).
    TextView* OnLink(Listener fn);
    // code_block_actions(..): the row is absolutely placed at the block's
    // top right, over a muted plate, exactly where node.rs puts it.
    TextView* CodeBlockActions(CodeBlockActionsFn fn, void* data = nullptr);
    // `.plugin(..)`: a parser and a renderer for blocks this view knows how
    // to draw and markdown does not. They are offered every block in the
    // order they were added, and the first that claims one renders it.
    TextView* Plugin(Str name, MdPluginParseFn parse, MdPluginRenderFn render,
                     void* data = nullptr);
    El* IntoEl();

  private:
    // node.rs names no text colour on a paragraph, a heading or a table cell:
    // each takes whatever the container above it pushed, which is how a
    // blockquote greys everything inside it in one line. This is that
    // inherited colour, unset meaning the theme's plain foreground.
    Rgba blockFg = {};
    bool blockFgSet = false;
    Rgba BlockFg() const;

    // ─── SelectionFormat::Source ──────────────────────────────────────────
    //
    // node.rs rebuilds a selection's Markdown by walking the BlockNode tree
    // (`text_by_kind`, `list_selected_source`, `table_selected_source`); the
    // window's selection here knows only the flat list of painted runs, so
    // the walk happens as the tree is built and each run is handed the piece
    // of that reconstruction it is responsible for. These four fields are the
    // walk's state.
    //
    // The line prefix the block being built sits under — one `> ` per
    // enclosing blockquote, and the indent under each enclosing list marker.
    Str srcLinePre = {};
    // What the next block's first line carries in front of that prefix: a
    // list item's `- ` or `1. `, spent by the first block of the item.
    Str srcMarker = {};
    // The Markdown marker the list being built hands its next item, which is
    // not the bullet glyph the item is drawn with.
    Str srcItemMarker = {};
    // The part of that marker the item's later lines are indented by:
    // list_selected_source indents by the marker alone, so a task item's
    // `[x] ` sits on the first line and not under the ones below it.
    Str srcItemPad = {};
    // Whether the item being built sits inside a task list item.
    // render_list_item_row draws no bullet or number for a row whose
    // enclosing item was a checkbox (`options.todo`), which is how a plain
    // list nested under a todo reads as part of it.
    bool inTodo = false;
    // The block runs are being built for, shared by every run in it, and
    // whether the next run starts a line rather than continuing one.
    const SelBlock* srcBlock = nullptr;
    bool srcLineStart = true;
    // The mark group open right now, so an adjacent run that carries the same
    // marks shares its record and the copier wraps the phrase once.
    const SelSource* srcRunLast = nullptr;
    uint8_t srcRunMarks = 0;
    Str srcRunHref = {};
    // Open a block: `marker` goes in front of its first line, `post` closes
    // it, and `join` says it continues the previous block's line (a table
    // cell). Answers null when the view is not selectable, since nothing
    // then paints a run that could be copied.
    const SelBlock* SrcOpen(Str marker, Str post, bool join = false);
    // One table cell: `| ` in front, ` |` or a separator after, and the
    // alignment row behind the last cell of the header.
    void SrcCell(MdNode* row, MdNode* c, int nCols, const uint8_t* colAlign);
    // Hand `t` the Markdown around it — wrap_with_mark's affixes — and
    // whether it continues the run before it. Answers `t`.
    El* SrcMark(El* t, uint8_t marks, Str href = {});
    // The next run starts a line of its own: a hard break, a new code line.
    void SrcBreak();
    // Hand an inline image element its `![alt](url)` — node.rs
    // image_markdown — as a run of its own with no text in it. Answers `e`.
    El* SrcImage(El* e, MdRun* r);
    // The text a plugin's parser sees, and the block a plugin claimed.
    Str BlockText(MdNode* n);
    El* PluginBlock(MdNode* n);
    // node.rs render_scroll_table: the same table, measured and scrolling.
    El* ScrollTable(MdNode* n);
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
    El* ImageRun(MdRun* r, float font, Rgba color, bool inFlow);
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
