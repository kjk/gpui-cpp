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
     - Children are an `ArenaVec<Node*>` from the arena the caller passed to
       `ToMdast`; nothing is freed one node at a time. */

#ifndef GPUI_MARKDOWN_MDAST_H_
#define GPUI_MARKDOWN_MDAST_H_

#include "base.h"

namespace markdown {

using base::Arena;
using base::ArenaStr;
using base::ArenaStrGet;
using base::ArenaVec;
using base::kArenaStrNone;
using base::Str;

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

struct Node {
    NodeKind kind = NodeKind::Root;

    // Root, Paragraph, Heading, Blockquote, List, ListItem, Emphasis, Strong,
    // Link, LinkReference, FootnoteDefinition, Table, TableRow, TableCell,
    // Delete.
    ArenaVec<Node*> children = {};

    // Every node has one once the parse is done; `hasPosition` is Rust's
    // `Option<Position>`, which is None only for a node built by hand.
    UnistPosition position = {};
    bool hasPosition = false;

    // The eight strings a node may carry. They are ArenaStr rather than Str
    // because a Node is allocated by the thousand and holds all eight
    // whichever kind it is: sixteen bytes each of pointer and length is half
    // the struct, spent pointing into the arena the node is already in.
    // `NodeStr(a, ..)` reads one back.
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

    // Table.
    ArenaVec<AlignKind> align = {};

    // List: the number the first item counts from. Optional (unordered lists
    // have none).
    uint32_t start = 0;
    bool hasStart = false;

    // Heading: 1..=6.
    uint8_t depth = 0;
    // LinkReference, ImageReference.
    ReferenceKind referenceKind = ReferenceKind::Shortcut;
    // List.
    bool ordered = false;
    // List, ListItem.
    bool spread = false;
    // ListItem: the GFM task list checkbox. Optional.
    bool checked = false;
    bool hasChecked = false;
};

Node* NodeNew(Arena* a, NodeKind kind);

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
