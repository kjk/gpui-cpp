/* src/mdast.rs — the syntax tree `ToMdast` hands back.

   Part of the C++ port of the `markdown` crate 1.0.0 (see
   src/markdown/readme.md).

   Deviations from the Rust:
     - Rust's `Node` is an enum of 30-odd structs. Here it is one struct with
       a `kind` and the union of their fields, the way ui/text.h's MdNode is
       one struct: C++ without RTTI reads a tag anyway, and every field is a
       word or two.
     - `Option<String>` is a `Str` whose `s` is null when the field is
       `None`. `Some("")` keeps a non-null pointer, which `StrOwn` in util.h
       guarantees.
     - Children are an `ArenaVec<ArenaNode>` — four-byte offsets into the
       arena the caller passed to `ToMdast`; nothing is freed one node at a
       time. */

#ifndef GPUI_MARKDOWN_MDAST_H_
#define GPUI_MARKDOWN_MDAST_H_

#include "base.h"

namespace markdown {

using base::Arena;
using base::ArenaPtr;
using base::ArenaPtrGet;
using base::ArenaPtrOf;
using base::ArenaStr;
using base::ArenaStrGet;
using base::ArenaVec;
using base::kArenaStrNone;
using base::Str;

struct Node;

// A link from a node to one of its children. Four bytes rather than eight:
// a tree of a few thousand nodes is a few thousand of these plus the
// ArenaVec segments holding them, and every one of them points into the same
// arena the node itself is in.
using ArenaNode = ArenaPtr<Node>;

// unist::Point.
struct UnistPoint {
    int32_t line = 1;
    int32_t column = 1;
    int32_t offset = 0;
};

// unist::Position. A node does not store one — it stores the two byte
// offsets, and `GetUnistPosition` counts the lines and columns back out of
// the source for whatever asks.
struct UnistPosition {
    UnistPoint start = {};
    UnistPoint end = {};
};

// The lines and columns the two offsets fall on, counted from the start of
// `md` — which is what the tokenizer did on the way past, and by the same
// rules: a CR that an LF follows is not a character, a tab runs to the next
// stop four columns apart, and every other byte is a column of its own, so a
// multi-byte character is as many columns as it is bytes.
//
// Two things this cannot recover, both of them positions the tokenizer could
// name and no node ever starts at: a point part-way through a tab's
// expansion comes back as the tab's own column, and `md` must be the same
// bytes the parse was given or the answer is a fiction.
UnistPosition GetUnistPosition(Str md, uint32_t start, uint32_t end);

// mdast.rs ReferenceKind.
enum class ReferenceKind : uint8_t {
    Shortcut,
    Collapsed,
    Full,
};

// mdast.rs AlignKind.
enum class AlignKind : uint8_t {
    Left,
    Right,
    Center,
    None,
};

// mdast.rs Node, one variant per member. MdxJsxFlowElement, MdxJsxTextElement,
// MdxjsEsm, MdxFlowExpression and MdxTextExpression are not here: the MDX
// constructs are not ported.
enum class NodeKind : uint8_t {
    Root,
    Blockquote,
    FootnoteDefinition,
    List,
    Toml,
    Yaml,
    Break,
    InlineCode,
    InlineMath,
    Delete,
    Emphasis,
    FootnoteReference,
    Html,
    Image,
    ImageReference,
    Link,
    LinkReference,
    Strong,
    Text,
    Code,
    Math,
    Heading,
    Table,
    ThematicBreak,
    TableRow,
    TableCell,
    ListItem,
    Definition,
    Paragraph,
};

// mdast.rs's `Option<bool>` and the flags beside it, packed into one byte.
// Six bools laid out one per byte cost eight of them once the struct is
// padded; a Node is allocated by the thousand, so they are a mask instead.
enum NodeFlag : uint8_t {
    // Every node has a position once the parse is done. Rust's
    // `Option<Position>` is None only for a node built by hand.
    NodeHasPosition = 1 << 0,
    // List: whether `start` says anything. Unordered lists count from
    // nothing, which is Rust's `Option<u32>` being None.
    NodeHasStart = 1 << 1,
    // List.
    NodeOrdered = 1 << 2,
    // List, ListItem.
    NodeSpread = 1 << 3,
    // ListItem: the GFM task list checkbox, and whether there is one.
    // The pair is Rust's `Option<bool>`.
    NodeChecked = 1 << 4,
    NodeHasChecked = 1 << 5,
};

// The fields are ordered largest first: two 24-byte members, then the eight
// strings and the three numbers at four bytes each, then three single bytes
// and one of tail padding. 96 bytes, where the order they were written in
// with a Str per string and a full unist Position cost 232.
struct Node {
    // Root, Paragraph, Heading, Blockquote, List, ListItem, Emphasis, Strong,
    // Link, LinkReference, FootnoteDefinition, Table, TableRow, TableCell,
    // Delete. `NodeKids(a, n)` walks them, `NodeChild(a, n, i)` indexes.
    ArenaVec<ArenaNode> children = {};

    // Table.
    ArenaVec<AlignKind> align = {};

    // Where the node came from: the byte offset into the source the parse
    // was given, and how many bytes it runs for. `NodeHasPosition` says
    // whether they mean anything, `NodeSrcEnd` is one past the last byte,
    // and `NodeSetSrcEnd` is how the length is written.
    //
    // Rust keeps a line, a column and an offset at each end, which is
    // twenty-four bytes on every node in the tree for something only a
    // diagnostic ever reads. The offsets are what the parse actually works
    // in; `GetUnistPosition(md, start, end)` counts the rest back out of the
    // source at the point something wants to print it.
    //
    // The length is sixteen bits and saturates: a node spanning more than
    // 65535 bytes — a document, or a code block the size of one — reports
    // 65535 and nothing says it was cut. Everything that reads a length here
    // is a diagnostic, and the constructs whose extent is acted on are all
    // far shorter than that.
    uint32_t srcStart = 0;
    uint16_t srcLen = 0;

    // The eight strings a node may carry. They are ArenaStr rather than Str
    // because a Node is allocated by the thousand and holds all eight
    // whichever kind it is: sixteen bytes each of pointer and length would be
    // two thirds of the struct, spent on lengths and on pointing into the
    // arena the node is already in. Four bytes each of offset, with the
    // length varint-encoded beside the characters. `NodeStr(a, ..)` reads one
    // back.
    //
    // Text, Html, Code, Math, InlineCode, InlineMath, Yaml, Toml.
    ArenaStr value = kArenaStrNone;
    // Link, Image, Definition.
    ArenaStr url = kArenaStrNone;
    // Link, Image, Definition. Optional.
    ArenaStr title = kArenaStrNone;
    // Image, ImageReference.
    ArenaStr alt = kArenaStrNone;
    // Definition, LinkReference, ImageReference, FootnoteDefinition,
    // FootnoteReference.
    ArenaStr identifier = kArenaStrNone;
    // The same five. Optional.
    ArenaStr label = kArenaStrNone;
    // Code: the fence's info word and the rest of the fence. Math: the rest.
    // Both optional.
    ArenaStr lang = kArenaStrNone;
    ArenaStr meta = kArenaStrNone;

    // Two fields that cannot both be live, so they are one. A List's
    // `start` is the number its first item counts from, and `NodeHasStart`
    // says whether it has one; a Heading's is its level, 1..=6. No node is
    // both kinds, which is what makes the fusing safe rather than clever —
    // `kind` says which of the two a given node means.
    uint32_t startOrDepth = 0;

    NodeKind kind = NodeKind::Root;
    // LinkReference, ImageReference.
    ReferenceKind referenceKind = ReferenceKind::Shortcut;
    // The NodeFlag bits.
    uint8_t flags = 0;

    bool Has(NodeFlag f) const { return (flags & f) != 0; }
    void Set(NodeFlag f, bool on) {
        flags = on ? (uint8_t)(flags | f) : (uint8_t)(flags & ~f);
    }
};

// The packing is the point, so it is checked rather than hoped for: two
// 24-byte members, eight 4-byte strings, two 4-byte numbers, the 2-byte
// length and three single bytes, rounded up to the 8 the ArenaVecs align
// to. Three bytes of that last 8 are padding — three more single-byte
// fields, or one more 2-byte one, would cost nothing, where a field put
// anywhere else costs eight a node and would otherwise go unnoticed.
static_assert(sizeof(Node) == 2 * 24 + (8 + 2) * 4 + 8,
              "Node has picked up padding; order the fields largest first");

// The longest span a node can name. A node that runs further reports this,
// which is a length that means "at least".
constexpr uint16_t kNodeSrcLenMax = 0xffff;

// One past the last byte the node covers.
inline uint32_t NodeSrcEnd(const Node* n) {
    return n->srcStart + n->srcLen;
}

// Where the node stops, written as the length it is kept as.
inline void NodeSetSrcEnd(Node* n, uint32_t end) {
    uint32_t len = end > n->srcStart ? end - n->srcStart : 0;
    n->srcLen = len > kNodeSrcLenMax ? kNodeSrcLenMax : (uint16_t)len;
}

// The start moves forward and the end stays put, which is what dropping
// bytes off the front of a node does. A length holds the distance to the
// end, so it has to come down by what the start went up by — the one thing
// an end offset did for free.
inline void NodeMoveSrcStart(Node* n, uint32_t to) {
    uint32_t by = to > n->srcStart ? to - n->srcStart : 0;
    n->srcLen = by >= n->srcLen ? 0 : (uint16_t)(n->srcLen - by);
    n->srcStart = to;
}

Node* NodeNew(Arena* a, NodeKind kind);

// Appending a child, which is where the offset is taken.
inline void NodeAddChild(Arena* a, Node* parent, Node* child) {
    parent->children.Append(a, ArenaPtrOf(a, child));
}

// The `i`th child, or null if there is no such child.
inline Node* NodeChild(Arena* a, const Node* n, int i) {
    if (i < 0 || i >= n->children.len) {
        return nullptr;
    }
    return ArenaPtrGet(a, n->children[i]);
}

// The children as something a range-for reads:
//
//     for (Node* child : NodeKids(a, n)) { ... }
//
// The offsets are resolved one at a time as the walk reaches them, so this
// is the ArenaVec walk with a lookup on the dereference and nothing else.
struct NodeKidsRange {
    Arena* a;
    ArenaVec<ArenaNode>::Iter it;
    ArenaVec<ArenaNode>::Iter last;

    struct Iter {
        Arena* a;
        ArenaVec<ArenaNode>::Iter it;

        Node* operator*() const { return ArenaPtrGet(a, *it); }
        Iter& operator++() {
            ++it;
            return *this;
        }
        bool operator!=(const Iter& o) const { return it != o.it; }
    };

    Iter begin() const { return Iter{a, it}; }
    Iter end() const { return Iter{a, last}; }
};

inline NodeKidsRange NodeKids(Arena* a, const Node* n) {
    return NodeKidsRange{a, n->children.begin(), n->children.end()};
}

// One of a node's eight strings, read out of the arena it was parsed into.
// The Str points into that arena and lives exactly as long as it does.
inline Str NodeStr(Arena* a, ArenaStr s) {
    return ArenaStrGet(a, s);
}

// mdast.rs `Node::children()`: whether this kind holds children at all.
bool NodeHasChildren(NodeKind kind);

// mdast.rs `ToString for Node`: the node's text, its children's concatenated.
// `to_mdast` uses it to build an image's alt text.
Str NodeToString(Arena* a, const Node* node);

} // namespace markdown

#endif // GPUI_MARKDOWN_MDAST_H_
