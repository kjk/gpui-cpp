/* src/mdast.rs — the tree's few methods.

   Part of the C++ port of markdown-rs 1.0.0 (see src/markdown/readme.md). */

#include "markdown/constant.h"
#include "markdown/mdast.h"

namespace markdown {

using base::Alloc;

Node* NodeNew(Arena* a, NodeKind kind) {
    Node* node = base::ArenaNew<Node>(a);
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
static ArenaStr NodeOwnValue(const Node* node) {
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
            return kArenaStrNone;
    }
}

// The lengths are in the words themselves, so the first pass reads no string
// bytes — only the arena lookups the child offsets need.
static int32_t NodeToStringLen(Arena* a, const Node* node) {
    if (NodeHasChildren(node->kind)) {
        int32_t len = 0;
        for (const Node* child : NodeKids(a, node)) {
            len += NodeToStringLen(a, child);
        }
        return len;
    }
    return (int32_t)base::ArenaStrLen(a, NodeOwnValue(node));
}

static int32_t NodeToStringFill(Arena* a, const Node* node, char* out,
                                int32_t at) {
    if (NodeHasChildren(node->kind)) {
        for (const Node* child : NodeKids(a, node)) {
            at = NodeToStringFill(a, child, out, at);
        }
        return at;
    }
    Str value = NodeStr(a, NodeOwnValue(node));
    if (value.len > 0) {
        memcpy(out + at, value.s, (size_t)value.len);
        at += value.len;
    }
    return at;
}

Str NodeToString(Arena* a, const Node* node) {
    // Two passes rather than a builder: the tree is already there, and this
    // way the result is one allocation of exactly the right size.
    int32_t len = NodeToStringLen(a, node);
    char* out = (char*)Alloc(a, len + 1);
    if (!out) {
        return {};
    }
    int32_t at = NodeToStringFill(a, node, out, 0);
    out[at] = 0;
    return Str(out, at);
}

// Walking the source once, by the tokenizer's own rules — see the header for
// which of them matter. The two offsets are visited in the one pass, so a
// position costs a scan of the source up to its end and nothing per node.
UnistPosition GetUnistPosition(Str md, uint32_t start, uint32_t end) {
    UnistPosition out;
    int32_t line = 1;
    int32_t column = 1;
    int32_t at = 0;
    int32_t stop = (int32_t)end;
    if (!md.s) {
        return out;
    }
    if (stop > md.len) {
        stop = md.len;
    }
    bool haveStart = false;
    while (at <= stop) {
        if (!haveStart && at == (int32_t)start) {
            out.start = UnistPoint{line, column, at};
            haveStart = true;
        }
        if (at == stop) {
            break;
        }
        uint8_t byte = (uint8_t)md.s[at];
        if (byte == '\r' && at + 1 < md.len && md.s[at + 1] == '\n') {
            // Not a character: the LF behind it is the line ending.
            at += 1;
            continue;
        }
        if (byte == '\n' || byte == '\r') {
            line += 1;
            column = 1;
            at += 1;
            continue;
        }
        if (byte == '\t') {
            int32_t remainder = column % kTabSize;
            column += remainder == 0 ? 1 : 1 + kTabSize - remainder;
            at += 1;
            continue;
        }
        column += 1;
        at += 1;
    }
    if (!haveStart) {
        // Past the end of the source, which a well-formed offset is not.
        out.start = UnistPoint{line, column, at};
    }
    out.end = UnistPoint{line, column, at};
    return out;
}

} // namespace markdown
