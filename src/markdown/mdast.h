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

// A Table's column alignments, which is the one list a node holds that is
// not a list of nodes: a count and two bits a column, in one arena
// allocation the node names by offset.
//
//     [varint count][2 bits a column, four to a byte, lowest bits first]
//
// A vector of them was 24 bytes in every node for something only Tables
// have, and a byte a column in an arena segment of its own. This is four
// bytes in the node and, for the eight-column table that is already wide,
// three bytes in the arena: one for the count, two for the columns. The
// whole list is known at the moment the table is entered, so it is counted,
// allocated once and filled — a vector's growth was never needed.
using ArenaAlign = uint32_t;
constexpr ArenaAlign kArenaAlignNone = 0;

static_assert((int)AlignKind::None < 4, "an alignment has to fit in 2 bits");

// `count` columns, all of them AlignKind::Left until set.
ArenaAlign ArenaAlignNew(Arena* a, int32_t count);
int32_t ArenaAlignCount(Arena* a, ArenaAlign al);
AlignKind ArenaAlignAt(Arena* a, ArenaAlign al, int32_t i);
void ArenaAlignSet(Arena* a, ArenaAlign al, int32_t i, AlignKind k);

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

// Everything in here is four bytes or fewer now, so the order is the eight
// strings and the four offsets, then the length and three single bytes with
// three of padding behind them. 56 bytes, where the order they were written
// in with a Str per string, a child vector, an alignment vector and a full
// unist Position cost 256 — and where holding one pointer would put the
// whole struct back on an eight-byte alignment.
struct Node {
    // The children, as a ring rather than a vector. `lastKid` is the last of
    // them and every child points at the one after it, the last one back at
    // the first — so the first child is `lastKid->sibling`, appending is
    // three stores, and a node with no children is one word of nothing.
    //
    // A vector cost 24 bytes in the node and an arena segment per node that
    // had any; this is 4 here and 4 in each child, which the child was going
    // to be padded out to anyway. `NodeKids(a, n)` walks them,
    // `NodeChild(a, n, i)` indexes, `NodeChildCount(a, n)` counts.
    //
    // A ring and not a plain list because appending is what the parse does
    // and nothing else: a plain list would need the parent to hold the first
    // and the last, or every append to walk to the end.
    ArenaNode lastKid = {};

    // The next child of whatever holds this one, wrapping round to the first
    // — meaningless on a node no `lastKid` names, which is the root and
    // nothing else.
    ArenaNode sibling = {};

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

    // The one word whose meaning `kind` decides. Three fields that can
    // never be live together, so they are one:
    //
    //   - List: the number its first item counts from, `NodeHasStart`
    //     saying whether it has one.
    //   - Heading: its level, 1..=6.
    //   - Table: the column alignments as an `ArenaAlign`, or
    //     `kArenaAlignNone`. `ArenaAlignAt(a, n->perKind, col)` reads one.
    //
    // A node is one of those kinds or none of them, which is what makes the
    // fusing safe rather than clever. Anything else added here has to be as
    // exclusive: the compiler cannot check it, so `kind` has to be looked at
    // before this is read, and every reader below does.
    uint32_t perKind = 0;

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

// The packing is the point, so it is checked rather than hoped for: eight
// 4-byte strings, four 4-byte offsets, the 2-byte length and three single
// bytes, rounded up to the 4 everything in here now aligns to. Three bytes
// of that last 8 are padding — three more single-byte fields, or one more
// 2-byte one, would cost nothing, where a field of eight bytes would cost
// eight more than itself and would otherwise go unnoticed.
static_assert(sizeof(Node) == (8 + 4) * 4 + 8,
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

// Appending a child: the new one takes the old last's place in the ring,
// which is the whole of what the parse does to a child list.
inline void NodeAddChild(Arena* a, Node* parent, Node* child) {
    ArenaNode at = ArenaPtrOf(a, child);
    if (parent->lastKid.IsSet()) {
        Node* last = ArenaPtrGet(a, parent->lastKid);
        child->sibling = last->sibling;
        last->sibling = at;
    } else {
        // A ring of one points at itself.
        child->sibling = at;
    }
    parent->lastKid = at;
}

// The last child, or null. The ring is arranged for this one to be free,
// because "the child that is still being built" is what the parse asks for.
inline Node* NodeLastChild(Arena* a, const Node* n) {
    return n->lastKid.IsSet() ? ArenaPtrGet(a, n->lastKid) : nullptr;
}

// The first child as the offset the ring holds it by: one step past the
// last. Nothing here needs to know where a node is to walk from it, which is
// what keeps the walks off `ArenaPtrOf` and its block scan.
inline ArenaNode NodeFirstKid(Arena* a, const Node* n) {
    Node* last = NodeLastChild(a, n);
    return last ? last->sibling : ArenaNode{};
}

// The first child, or null.
inline Node* NodeFirstChild(Arena* a, const Node* n) {
    ArenaNode first = NodeFirstKid(a, n);
    return first.IsSet() ? ArenaPtrGet(a, first) : nullptr;
}

// The `i`th child, or null if there is no such child. A walk, where the
// vector was an index — every caller here asks for the first or the last.
inline Node* NodeChild(Arena* a, const Node* n, int i) {
    if (i < 0 || !n->lastKid.IsSet()) {
        return nullptr;
    }
    Node* last = ArenaPtrGet(a, n->lastKid);
    Node* at = ArenaPtrGet(a, last->sibling);
    for (int k = 0; k < i; k++) {
        if (at == last) {
            return nullptr;
        }
        at = ArenaPtrGet(a, at->sibling);
    }
    return at;
}

// How many children, which is a walk and so is not what a loop should test
// against.
inline int NodeChildCount(Arena* a, const Node* n) {
    if (!n->lastKid.IsSet()) {
        return 0;
    }
    Node* last = ArenaPtrGet(a, n->lastKid);
    int count = 1;
    for (Node* at = ArenaPtrGet(a, last->sibling); at != last;
         at = ArenaPtrGet(a, at->sibling)) {
        count++;
    }
    return count;
}

// The children as something a range-for reads:
//
//     for (Node* child : NodeKids(a, n)) { ... }
//
// The ring has no end of its own, so the walk carries the last child and
// stops on the step off it — which is what makes the iterator hold two
// offsets where a vector's held one.
struct NodeKidsRange {
    Arena* a;
    ArenaNode at;
    ArenaNode last;

    struct Iter {
        Arena* a;
        ArenaNode at;
        ArenaNode last;

        Node* operator*() const { return ArenaPtrGet(a, at); }
        Iter& operator++() {
            at = at == last ? ArenaNode{} : ArenaPtrGet(a, at)->sibling;
            return *this;
        }
        bool operator!=(const Iter& o) const { return at != o.at; }
    };

    Iter begin() const { return Iter{a, at, last}; }
    Iter end() const { return Iter{a, ArenaNode{}, last}; }
};

inline NodeKidsRange NodeKids(Arena* a, const Node* n) {
    return NodeKidsRange{a, NodeFirstKid(a, n), n->lastKid};
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
