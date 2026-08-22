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

// unist::Position.
struct UnistPosition {
    UnistPoint start = {};
    UnistPoint end = {};
};

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

// The fields are ordered largest first: three 24-byte members, then the
// eight strings and the one number at four bytes each, then three single
// bytes and one of tail padding. 112 bytes, where the order they were
// written in with a Str per string cost 232.
struct Node {
    // Root, Paragraph, Heading, Blockquote, List, ListItem, Emphasis, Strong,
    // Link, LinkReference, FootnoteDefinition, Table, TableRow, TableCell,
    // Delete. `NodeKids(a, n)` walks them, `NodeChild(a, n, i)` indexes.
    ArenaVec<ArenaNode> children = {};

    // Table.
    ArenaVec<AlignKind> align = {};

    // Where the node came from in the source. `NodeHasPosition` says whether
    // it means anything.
    UnistPosition position = {};

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

// The packing is the point, so it is checked rather than hoped for: three
// 24-byte members, eight 4-byte strings, one 4-byte number and three single
// bytes, rounded up to the 8 the ArenaVecs align to. The arithmetic below
// still reads 4 for that last group because the byte fusing start and depth
// freed went into the padding rather than off the struct — one more
// single-byte field would cost nothing, and a field put anywhere else costs
// eight a node, which would otherwise go unnoticed.
static_assert(sizeof(Node) == 3 * 24 + 8 * 4 + 4 + 4,
              "Node has picked up padding; order the fields largest first");

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
