/* src/mdast.rs — the tree's few methods.

   Part of the C++ port of markdown-rs 1.0.0 (see src/markdown/readme.md). */

#include "markdown/mdast.h"

namespace markdown {

using gpui::Alloc;

Node* NodeNew(Arena* a, NodeKind kind) {
    Node* node = gpui::ArenaNew<Node>(a);
    node->kind = kind;
    return node;
}

bool NodeHasChildren(NodeKind kind) {
    switch (kind) {
        case NodeKind::Root:
        case NodeKind::Paragraph:
        case NodeKind::Heading:
        case NodeKind::Blockquote:
        case NodeKind::List:
        case NodeKind::ListItem:
        case NodeKind::Emphasis:
        case NodeKind::Strong:
        case NodeKind::Link:
        case NodeKind::LinkReference:
        case NodeKind::FootnoteDefinition:
        case NodeKind::Table:
        case NodeKind::TableRow:
        case NodeKind::TableCell:
        case NodeKind::Delete:
            return true;
        default:
            return false;
    }
}

// The value a node contributes to `NodeToString`: its own, or nothing when
// its children are what it is made of.
static Str NodeOwnValue(const Node* node) {
    switch (node->kind) {
        case NodeKind::Toml:
        case NodeKind::Yaml:
        case NodeKind::InlineCode:
        case NodeKind::InlineMath:
        case NodeKind::Html:
        case NodeKind::Text:
        case NodeKind::Code:
        case NodeKind::Math:
            return node->value;
        default:
            return {};
    }
}

static int32_t NodeToStringLen(const Node* node) {
    if (NodeHasChildren(node->kind)) {
        int32_t len = 0;
        for (int32_t i = 0; i < node->children.len; i++) {
            len += NodeToStringLen(node->children[i]);
        }
        return len;
    }
    return NodeOwnValue(node).len;
}

static int32_t NodeToStringFill(const Node* node, char* out, int32_t at) {
    if (NodeHasChildren(node->kind)) {
        for (int32_t i = 0; i < node->children.len; i++) {
            at = NodeToStringFill(node->children[i], out, at);
        }
        return at;
    }
    Str value = NodeOwnValue(node);
    if (value.len > 0) {
        memcpy(out + at, value.s, (size_t)value.len);
        at += value.len;
    }
    return at;
}

Str NodeToString(Arena* a, const Node* node) {
    // Two passes rather than a builder: the tree is already there, and this
    // way the result is one allocation of exactly the right size.
    int32_t len = NodeToStringLen(node);
    char* out = (char*)Alloc(a, len + 1);
    if (!out) {
        return {};
    }
    int32_t at = NodeToStringFill(node, out, 0);
    out[at] = 0;
    return Str(out, at);
}

} // namespace markdown
