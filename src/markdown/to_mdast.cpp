/* src/to_mdast.rs — events to a syntax tree.

   Part of the C++ port of markdown-rs 1.0.0 (see src/markdown/readme.md).

   Deviations from the Rust:
     - `Result` is gone. Every error the crate can raise here is about MDX
       tags, and MDX is not ported, so a compile always succeeds. That takes
       `on_mismatch_error` and the event-name check in `tail_pop` with it.
     - `on_exit_media` rewrites the node it finds in place instead of popping
       it and pushing a new one: a `Node` is one struct here, so a link and a
       link reference differ by a field. */

#include "markdown/construct.h"

namespace markdown {

using base::Alloc;

// to_mdast.rs Reference. `reference_kind: Option<ReferenceKind>` is the kind
// plus its flag.
struct Reference {
    ReferenceKind kind = ReferenceKind::Shortcut;
    bool kindSome = true;
    Str identifier = {};
    Str label = {};
};

// One entry of `trees`: the tree being built, the path to the node being
// filled, and the events that opened them.
struct TreeFrame {
    Node* tree = nullptr;
    ArenaVec<int32_t> stack = {};
    ArenaVec<int32_t> eventStack = {};
};

struct CompileContext {
    Arena* a = nullptr;
    const Vec<Event>* events = nullptr;
    Str bytes = {};
    uint8_t characterReferenceMarker = 0;
    bool gfmTableInside = false;
    bool hardBreakAfter = false;
    bool headingSetextTextAfter = false;
    Vec<Reference> mediaReferenceStack;
    bool rawFlowFenceSeen = false;
    Vec<TreeFrame> trees;
    int32_t index = 0;
};

// ─── strings ─────────────────────────────────────────────────────────────

// `String::push_str`.
static Str StrCat(Arena* a, Str head, Str tail) {
    int32_t len = head.len + tail.len;
    char* out = (char*)Alloc(a, len + 1);
    if (!out) {
        return head;
    }
    if (head.len > 0) {
        memcpy(out, head.s, (size_t)head.len);
    }
    if (tail.len > 0) {
        memcpy(out + head.len, tail.s, (size_t)tail.len);
    }
    out[len] = 0;
    return Str(out, len);
}

// `normalize_identifier(..).to_lowercase()`, which is what an identifier on
// the tree is. ASCII only, as in util.h.
static Str IdentifierFrom(Arena* a, Str value) {
    Str id = NormalizeIdentifier(a, value);
    for (int32_t i = 0; i < id.len; i++) {
        if (id.s[i] >= 'A' && id.s[i] <= 'Z') {
            id.s[i] = (char)(id.s[i] + 32);
        }
    }
    return id;
}

// to_mdast.rs `trim_eol`.
static Str TrimEol(Str value, bool atStart, bool atEnd) {
    int32_t start = 0;
    int32_t end = value.len;
    if (atStart && value.len > 0) {
        if (value.s[0] == '\n') {
            start += 1;
        } else if (value.s[0] == '\r') {
            start += 1;
            if (value.len > 1 && value.s[1] == '\n') {
                start += 1;
            }
        }
    }
    if (atEnd && end > start) {
        if (value.s[end - 1] == '\n') {
            end -= 1;
            if (end > start && value.s[end - 1] == '\r') {
                end -= 1;
            }
        } else if (value.s[end - 1] == '\r') {
            end -= 1;
        }
    }
    return Str(value.s + start, end - start);
}

// ─── the tree stack ──────────────────────────────────────────────────────

static UnistPoint ToUnist(const Point& point) {
    UnistPoint out;
    out.line = point.line;
    out.column = point.column;
    out.offset = point.index;
    return out;
}

static UnistPosition PositionFromEvent(const Event& event) {
    UnistPosition position;
    position.start = ToUnist(event.point);
    position.end = position.start;
    return position;
}

static Node* DelveMut(Node* node, const ArenaVec<int32_t>& stack,
                      int32_t stackLen) {
    for (int32_t i = 0; i < stackLen; i++) {
        node = node->children[stack[i]];
    }
    return node;
}

static TreeFrame& TreeTail(CompileContext* c) {
    return c->trees[c->trees.len - 1];
}

static Node* TailMut(CompileContext* c) {
    TreeFrame& frame = TreeTail(c);
    return DelveMut(frame.tree, frame.stack, frame.stack.len);
}

static Node* TailPenultimateMut(CompileContext* c) {
    TreeFrame& frame = TreeTail(c);
    return DelveMut(frame.tree, frame.stack, frame.stack.len - 1);
}

static void Buffer(CompileContext* c) {
    TreeFrame frame;
    frame.tree = NodeNew(c->a, NodeKind::Paragraph);
    c->trees.Append(frame);
}

static Node* Resume(CompileContext* c) {
    TreeFrame frame = c->trees[--c->trees.len];
    return frame.tree;
}

static void TailPush(CompileContext* c, Node* child) {
    if (!child->hasPosition) {
        child->position = PositionFromEvent((*c->events)[c->index]);
        child->hasPosition = true;
    }
    TreeFrame& frame = TreeTail(c);
    Node* node = DelveMut(frame.tree, frame.stack, frame.stack.len);
    int32_t index = node->children.len;
    node->children.Append(c->a, child);
    frame.stack.Append(c->a, index);
    frame.eventStack.Append(c->a, c->index);
}

static void TailPushAgain(CompileContext* c) {
    TreeFrame& frame = TreeTail(c);
    Node* node = DelveMut(frame.tree, frame.stack, frame.stack.len);
    frame.stack.Append(c->a, node->children.len - 1);
    frame.eventStack.Append(c->a, c->index);
}

static void TailPop(CompileContext* c) {
    UnistPoint end = ToUnist((*c->events)[c->index].point);
    TreeFrame& frame = TreeTail(c);
    Node* node = DelveMut(frame.tree, frame.stack, frame.stack.len);
    node->position.end = end;
    frame.stack.Pop();
    frame.eventStack.Pop();
}

// ─── enter ───────────────────────────────────────────────────────────────

static void OnEnterBuffer(CompileContext* c) {
    Buffer(c);
}

static void OnEnterData(CompileContext* c) {
    Node* parent = TailMut(c);
    if (parent->children.len > 0 &&
        parent->children[parent->children.len - 1]->kind == NodeKind::Text) {
        TailPushAgain(c);
    } else {
        TailPush(c, NodeNew(c->a, NodeKind::Text));
    }
}

static void OnEnterAutolink(CompileContext* c) {
    TailPush(c, NodeNew(c->a, NodeKind::Link));
}

static void OnEnterCodeFenced(CompileContext* c) {
    TailPush(c, NodeNew(c->a, NodeKind::Code));
}

static void OnEnterGfmAutolinkLiteral(CompileContext* c) {
    OnEnterAutolink(c);
    OnEnterData(c);
}

static void OnEnterList(CompileContext* c) {
    Node* node = NodeNew(c->a, NodeKind::List);
    node->ordered = (*c->events)[c->index].name == Name::ListOrdered;
    node->spread = ListLoose(*c->events, c->index, false);
    TailPush(c, node);
}

static void OnEnterListItem(CompileContext* c) {
    Node* node = NodeNew(c->a, NodeKind::ListItem);
    node->spread = ListItemLoose(*c->events, c->index);
    TailPush(c, node);
}

static void OnEnterMedia(CompileContext* c, NodeKind kind) {
    TailPush(c, NodeNew(c->a, kind));
    Reference reference;
    c->mediaReferenceStack.Append(reference);
}

static void Enter(CompileContext* c) {
    switch ((*c->events)[c->index].name) {
        case Name::AutolinkEmail:
        case Name::AutolinkProtocol:
        case Name::CharacterEscapeValue:
        case Name::CharacterReference:
        case Name::CodeFlowChunk:
        case Name::CodeTextData:
        case Name::Data:
        case Name::FrontmatterChunk:
        case Name::HtmlFlowData:
        case Name::HtmlTextData:
        case Name::MathFlowChunk:
        case Name::MathTextData:
            OnEnterData(c);
            break;

        case Name::CodeFencedFenceInfo:
        case Name::CodeFencedFenceMeta:
        case Name::DefinitionDestinationString:
        case Name::DefinitionLabelString:
        case Name::DefinitionTitleString:
        case Name::GfmFootnoteDefinitionLabelString:
        case Name::LabelText:
        case Name::MathFlowFenceMeta:
        case Name::ReferenceString:
        case Name::ResourceDestinationString:
        case Name::ResourceTitleString:
            OnEnterBuffer(c);
            break;

        case Name::Autolink:
            OnEnterAutolink(c);
            break;
        case Name::BlockQuote:
            TailPush(c, NodeNew(c->a, NodeKind::Blockquote));
            break;
        case Name::CodeFenced:
            OnEnterCodeFenced(c);
            break;
        case Name::CodeIndented:
            OnEnterCodeFenced(c);
            OnEnterBuffer(c);
            break;
        case Name::CodeText:
            TailPush(c, NodeNew(c->a, NodeKind::InlineCode));
            Buffer(c);
            break;
        case Name::MathText:
            TailPush(c, NodeNew(c->a, NodeKind::InlineMath));
            Buffer(c);
            break;
        case Name::Definition:
            TailPush(c, NodeNew(c->a, NodeKind::Definition));
            break;
        case Name::Emphasis:
            TailPush(c, NodeNew(c->a, NodeKind::Emphasis));
            break;
        case Name::Frontmatter: {
            int32_t index = (*c->events)[c->index].point.index;
            TailPush(c, NodeNew(c->a, c->bytes.s[index] == '+'
                                          ? NodeKind::Toml
                                          : NodeKind::Yaml));
            Buffer(c);
            break;
        }
        case Name::GfmAutolinkLiteralEmail:
        case Name::GfmAutolinkLiteralMailto:
        case Name::GfmAutolinkLiteralProtocol:
        case Name::GfmAutolinkLiteralWww:
        case Name::GfmAutolinkLiteralXmpp:
            OnEnterGfmAutolinkLiteral(c);
            break;
        case Name::GfmFootnoteCall:
            OnEnterMedia(c, NodeKind::FootnoteReference);
            break;
        case Name::GfmFootnoteDefinition:
            TailPush(c, NodeNew(c->a, NodeKind::FootnoteDefinition));
            break;
        case Name::GfmStrikethrough:
            TailPush(c, NodeNew(c->a, NodeKind::Delete));
            break;
        case Name::GfmTable: {
            Node* node = NodeNew(c->a, NodeKind::Table);
            GfmTableAlign(*c->events, c->index, c->a, &node->align);
            TailPush(c, node);
            c->gfmTableInside = true;
            break;
        }
        case Name::GfmTableRow:
            TailPush(c, NodeNew(c->a, NodeKind::TableRow));
            break;
        case Name::GfmTableCell:
            TailPush(c, NodeNew(c->a, NodeKind::TableCell));
            break;
        case Name::HardBreakEscape:
        case Name::HardBreakTrailing:
            TailPush(c, NodeNew(c->a, NodeKind::Break));
            break;
        case Name::HeadingAtx:
        case Name::HeadingSetext:
            // `depth` is set later.
            TailPush(c, NodeNew(c->a, NodeKind::Heading));
            break;
        case Name::HtmlFlow:
        case Name::HtmlText:
            TailPush(c, NodeNew(c->a, NodeKind::Html));
            Buffer(c);
            break;
        case Name::Image:
            OnEnterMedia(c, NodeKind::Image);
            break;
        case Name::Link:
            OnEnterMedia(c, NodeKind::Link);
            break;
        case Name::ListItem:
            OnEnterListItem(c);
            break;
        case Name::ListOrdered:
        case Name::ListUnordered:
            OnEnterList(c);
            break;
        case Name::MathFlow:
            TailPush(c, NodeNew(c->a, NodeKind::Math));
            break;
        case Name::Paragraph:
            TailPush(c, NodeNew(c->a, NodeKind::Paragraph));
            break;
        case Name::Reference:
            c->mediaReferenceStack[c->mediaReferenceStack.len - 1].kind =
                ReferenceKind::Collapsed;
            c->mediaReferenceStack[c->mediaReferenceStack.len - 1].kindSome =
                true;
            break;
        case Name::Resource:
            c->mediaReferenceStack[c->mediaReferenceStack.len - 1].kindSome =
                false;
            break;
        case Name::Strong:
            TailPush(c, NodeNew(c->a, NodeKind::Strong));
            break;
        case Name::ThematicBreak:
            TailPush(c, NodeNew(c->a, NodeKind::ThematicBreak));
            break;
        default:
            break;
    }
}

// ─── exit ────────────────────────────────────────────────────────────────

static Slice ExitSlice(CompileContext* c) {
    Position position = PositionFromExitEvent(*c->events, c->index);
    return SliceFromPosition(c->bytes, position);
}

static void OnExit(CompileContext* c) {
    TailPop(c);
}

static void OnExitData(CompileContext* c) {
    Str value = ExitSlice(c).bytes;
    Node* node = TailMut(c);
    node->value = StrCat(c->a, node->value, value);
    OnExit(c);
}

static void OnExitAutolinkProtocol(CompileContext* c) {
    OnExitData(c);
    Str value = ExitSlice(c).bytes;
    Node* link = TailMut(c);
    link->url = StrCat(c->a, link->url, value);
}

static void OnExitAutolinkEmail(CompileContext* c) {
    OnExitData(c);
    Str value = ExitSlice(c).bytes;
    Node* link = TailMut(c);
    link->url = StrCat(c->a, link->url, Str("mailto:", 7));
    link->url = StrCat(c->a, link->url, value);
}

static void OnExitCharacterReferenceValue(CompileContext* c) {
    Str value = CharacterReferenceDecode(c->a, ExitSlice(c).bytes,
                                         c->characterReferenceMarker);
    Node* node = TailMut(c);
    node->value = StrCat(c->a, node->value, value);
    c->characterReferenceMarker = 0;
}

static void OnExitRawFlowFence(CompileContext* c) {
    if (!c->rawFlowFenceSeen) {
        Buffer(c);
        c->rawFlowFenceSeen = true;
    }
}

static void OnExitRawFlow(CompileContext* c) {
    Str value = TrimEol(NodeToString(c->a, Resume(c)), true, true);
    TailMut(c)->value = value;
    OnExit(c);
    c->rawFlowFenceSeen = false;
}

static void OnExitCodeIndented(CompileContext* c) {
    Str value = TrimEol(NodeToString(c->a, Resume(c)), false, true);
    TailMut(c)->value = value;
    OnExit(c);
    c->rawFlowFenceSeen = false;
}

static void OnExitRawText(CompileContext* c) {
    Str value = NodeToString(c->a, Resume(c));

    // In a table cell, `\|` is an escaped pipe.
    if (c->gfmTableInside) {
        int32_t index = 0;
        int32_t len = value.len;
        bool replace = false;
        char* bytes = value.s;
        while (index < len) {
            if (index + 1 < len && bytes[index] == '\\' &&
                bytes[index + 1] == '|') {
                replace = true;
                for (int32_t i = index; i + 1 < len; i++) {
                    bytes[i] = bytes[i + 1];
                }
                len -= 1;
            }
            index += 1;
        }
        if (replace) {
            value.len = len;
            value.s[len] = 0;
        }
    }

    // One space at either end is stripped, unless it is all spaces.
    if (value.len > 2 && value.s[0] == ' ' && value.s[value.len - 1] == ' ') {
        bool allSpaces = true;
        for (int32_t i = 0; i < value.len; i++) {
            if (value.s[i] != ' ') {
                allSpaces = false;
                break;
            }
        }
        if (!allSpaces) {
            value = Str(value.s + 1, value.len - 2);
        }
    }

    TailMut(c)->value = value;
    OnExit(c);
}

static void OnExitDefinitionId(CompileContext* c) {
    Str label = NodeToString(c->a, Resume(c));
    Str identifier = IdentifierFrom(c->a, ExitSlice(c).bytes);
    Node* node = TailMut(c);
    node->label = label;
    node->identifier = identifier;
}

static void OnExitGfmAutolinkLiteral(CompileContext* c) {
    OnExitData(c);
    Str value = ExitSlice(c).bytes;
    Name name = (*c->events)[c->index].name;
    Node* link = TailMut(c);
    if (name == Name::GfmAutolinkLiteralEmail) {
        link->url = StrCat(c->a, link->url, Str("mailto:", 7));
    } else if (name == Name::GfmAutolinkLiteralWww) {
        link->url = StrCat(c->a, link->url, Str("http://", 7));
    }
    link->url = StrCat(c->a, link->url, value);
    OnExit(c);
}

static void OnExitGfmTaskListItemValue(CompileContext* c) {
    bool checked =
        (*c->events)[c->index].name == Name::GfmTaskListItemValueChecked;
    Node* ancestor = TailPenultimateMut(c);
    ancestor->checked = checked;
    ancestor->hasChecked = true;
}

static void OnExitHeadingAtxSequence(CompileContext* c) {
    Node* node = TailMut(c);
    if (node->depth == 0) {
        node->depth = (uint8_t)ExitSlice(c).Len();
    }
}

static void OnExitHeadingSetextUnderlineSequence(CompileContext* c) {
    Position position = PositionFromExitEvent(*c->events, c->index);
    uint8_t head = (uint8_t)c->bytes.s[position.start.index];
    TailMut(c)->depth = head == '-' ? 2 : 1;
}

static void OnExitLabelText(CompileContext* c) {
    Node* fragment = Resume(c);
    Str label = NodeToString(c->a, fragment);
    Str identifier = IdentifierFrom(c->a, ExitSlice(c).bytes);

    Reference& reference =
        c->mediaReferenceStack[c->mediaReferenceStack.len - 1];
    reference.label = label;
    reference.identifier = identifier;

    Node* node = TailMut(c);
    if (node->kind == NodeKind::Link) {
        node->children = fragment->children;
        fragment->children = ArenaVec<Node*>{};
    } else if (node->kind == NodeKind::Image) {
        node->alt = label;
    }
}

static void OnExitLineEnding(CompileContext* c) {
    if (c->headingSetextTextAfter) {
        // Ignore.
        return;
    }
    if (c->hardBreakAfter) {
        UnistPoint end = ToUnist((*c->events)[c->index].point);
        Node* node = TailMut(c);
        Node* tail = node->children[node->children.len - 1];
        tail->position.end = end;
        c->hardBreakAfter = false;
        return;
    }
    NodeKind kind = TailMut(c)->kind;
    if (kind == NodeKind::Emphasis || kind == NodeKind::Heading ||
        kind == NodeKind::Paragraph || kind == NodeKind::Strong ||
        kind == NodeKind::Delete) {
        c->index -= 1;
        OnEnterData(c);
        c->index += 1;
        OnExitData(c);
    }
}

static void OnExitHtml(CompileContext* c) {
    Str value = NodeToString(c->a, Resume(c));
    TailMut(c)->value = value;
    OnExit(c);
}

static void OnExitMedia(CompileContext* c) {
    Reference reference =
        c->mediaReferenceStack[--c->mediaReferenceStack.len];
    OnExit(c);
    if (!reference.kindSome) {
        return;
    }
    Node* parent = TailMut(c);
    Node* node = parent->children[parent->children.len - 1];
    if (node->kind == NodeKind::FootnoteReference) {
        node->identifier = reference.identifier;
        node->label = reference.label;
    } else if (node->kind == NodeKind::Image) {
        node->kind = NodeKind::ImageReference;
        node->referenceKind = reference.kind;
        node->identifier = reference.identifier;
        node->label = reference.label;
        node->url = {};
        node->title = {};
    } else if (node->kind == NodeKind::Link) {
        node->kind = NodeKind::LinkReference;
        node->referenceKind = reference.kind;
        node->identifier = reference.identifier;
        node->label = reference.label;
        node->url = {};
        node->title = {};
    }
}

static void OnExitListItem(CompileContext* c) {
    Node* item = TailMut(c);
    if (item->hasChecked && item->children.len > 0 &&
        item->children[0]->kind == NodeKind::Paragraph) {
        Node* paragraph = item->children[0];
        if (paragraph->children.len > 0 &&
            paragraph->children[0]->kind == NodeKind::Text) {
            Node* text = paragraph->children[0];
            UnistPoint point = text->position.start;
            Str value = text->value;
            int32_t start = 0;
            if (value.len > 0 && (value.s[0] == '\t' || value.s[0] == ' ')) {
                point.offset += 1;
                point.column += 1;
                start += 1;
            } else if (value.len > 0 &&
                       (value.s[0] == '\r' || value.s[0] == '\n')) {
                point.line += 1;
                point.column = 1;
                point.offset += 1;
                start += 1;
                if (value.len > 1 && value.s[0] == '\r' && value.s[1] == '\n') {
                    point.offset += 1;
                    start += 1;
                }
            }
            if (start == value.len) {
                // Remove the empty text: the paragraph was only a checkbox.
                for (int32_t i = 0; i + 1 < paragraph->children.len; i++) {
                    paragraph->children[i] = paragraph->children[i + 1];
                }
                paragraph->children.Pop();
            } else {
                text->value = Str(value.s + start, value.len - start);
                text->position.start = point;
            }
            paragraph->position.start = point;
        }
    }
    OnExit(c);
}

static void OnExitListItemValue(CompileContext* c) {
    Str value = ExitSlice(c).bytes;
    uint32_t start = 0;
    for (int32_t i = 0; i < value.len; i++) {
        start = start * 10 + (uint32_t)(value.s[i] - '0');
    }
    Node* node = TailPenultimateMut(c);
    if (!node->hasStart) {
        node->start = start;
        node->hasStart = true;
    }
}

static void OnExitReferenceString(CompileContext* c) {
    Str label = NodeToString(c->a, Resume(c));
    Str identifier = IdentifierFrom(c->a, ExitSlice(c).bytes);
    Reference& reference =
        c->mediaReferenceStack[c->mediaReferenceStack.len - 1];
    reference.kind = ReferenceKind::Full;
    reference.kindSome = true;
    reference.label = label;
    reference.identifier = identifier;
}

static void Exit(CompileContext* c) {
    switch ((*c->events)[c->index].name) {
        case Name::Autolink:
        case Name::BlockQuote:
        case Name::CharacterReference:
        case Name::Definition:
        case Name::Emphasis:
        case Name::GfmFootnoteDefinition:
        case Name::GfmStrikethrough:
        case Name::GfmTableRow:
        case Name::GfmTableCell:
        case Name::HeadingAtx:
        case Name::ListOrdered:
        case Name::ListUnordered:
        case Name::Paragraph:
        case Name::Strong:
        case Name::ThematicBreak:
            OnExit(c);
            break;

        case Name::CharacterEscapeValue:
        case Name::CodeFlowChunk:
        case Name::CodeTextData:
        case Name::Data:
        case Name::FrontmatterChunk:
        case Name::HtmlFlowData:
        case Name::HtmlTextData:
        case Name::MathFlowChunk:
        case Name::MathTextData:
            OnExitData(c);
            break;

        case Name::AutolinkProtocol:
            OnExitAutolinkProtocol(c);
            break;
        case Name::AutolinkEmail:
            OnExitAutolinkEmail(c);
            break;
        case Name::CharacterReferenceMarker:
            c->characterReferenceMarker = '&';
            break;
        case Name::CharacterReferenceMarkerNumeric:
            c->characterReferenceMarker = '#';
            break;
        case Name::CharacterReferenceMarkerHexadecimal:
            c->characterReferenceMarker = 'x';
            break;
        case Name::CharacterReferenceValue:
            OnExitCharacterReferenceValue(c);
            break;
        case Name::CodeFencedFenceInfo:
            TailMut(c)->lang = NodeToString(c->a, Resume(c));
            break;
        case Name::CodeFencedFenceMeta:
        case Name::MathFlowFenceMeta:
            TailMut(c)->meta = NodeToString(c->a, Resume(c));
            break;
        case Name::CodeFencedFence:
        case Name::MathFlowFence:
            OnExitRawFlowFence(c);
            break;
        case Name::CodeFenced:
        case Name::MathFlow:
            OnExitRawFlow(c);
            break;
        case Name::CodeIndented:
            OnExitCodeIndented(c);
            break;
        case Name::CodeText:
        case Name::MathText:
            OnExitRawText(c);
            break;
        case Name::DefinitionDestinationString:
            TailMut(c)->url = NodeToString(c->a, Resume(c));
            break;
        case Name::DefinitionLabelString:
        case Name::GfmFootnoteDefinitionLabelString:
            OnExitDefinitionId(c);
            break;
        case Name::DefinitionTitleString:
            TailMut(c)->title = NodeToString(c->a, Resume(c));
            break;
        case Name::Frontmatter:
            TailMut(c)->value =
                TrimEol(NodeToString(c->a, Resume(c)), true, true);
            OnExit(c);
            break;
        case Name::GfmAutolinkLiteralEmail:
        case Name::GfmAutolinkLiteralMailto:
        case Name::GfmAutolinkLiteralProtocol:
        case Name::GfmAutolinkLiteralWww:
        case Name::GfmAutolinkLiteralXmpp:
            OnExitGfmAutolinkLiteral(c);
            break;
        case Name::GfmFootnoteCall:
        case Name::Image:
        case Name::Link:
            OnExitMedia(c);
            break;
        case Name::GfmTable:
            OnExit(c);
            c->gfmTableInside = false;
            break;
        case Name::GfmTaskListItemValueUnchecked:
        case Name::GfmTaskListItemValueChecked:
            OnExitGfmTaskListItemValue(c);
            break;
        case Name::HardBreakEscape:
        case Name::HardBreakTrailing:
            OnExit(c);
            c->hardBreakAfter = true;
            break;
        case Name::HeadingAtxSequence:
            OnExitHeadingAtxSequence(c);
            break;
        case Name::HeadingSetext:
            c->headingSetextTextAfter = false;
            OnExit(c);
            break;
        case Name::HeadingSetextUnderlineSequence:
            OnExitHeadingSetextUnderlineSequence(c);
            break;
        case Name::HeadingSetextText:
            c->headingSetextTextAfter = true;
            break;
        case Name::HtmlFlow:
        case Name::HtmlText:
            OnExitHtml(c);
            break;
        case Name::LabelText:
            OnExitLabelText(c);
            break;
        case Name::LineEnding:
            OnExitLineEnding(c);
            break;
        case Name::ListItem:
            OnExitListItem(c);
            break;
        case Name::ListItemValue:
            OnExitListItemValue(c);
            break;
        case Name::ReferenceString:
            OnExitReferenceString(c);
            break;
        case Name::ResourceDestinationString:
            TailMut(c)->url = NodeToString(c->a, Resume(c));
            break;
        case Name::ResourceTitleString:
            TailMut(c)->title = NodeToString(c->a, Resume(c));
            break;
        default:
            break;
    }
}

Node* ToMdastCompile(const Vec<Event>& events, ParseState* parseState) {
    CompileContext context;
    context.a = parseState->a;
    context.events = &events;
    context.bytes = parseState->bytes;

    TreeFrame frame;
    frame.tree = NodeNew(context.a, NodeKind::Root);
    frame.tree->hasPosition = true;
    if (events.len > 0) {
        frame.tree->position.start = ToUnist(events[0].point);
        frame.tree->position.end = ToUnist(events[events.len - 1].point);
    }
    context.trees.Append(frame);

    int32_t index = 0;
    while (index < events.len) {
        context.index = index;
        if (events[index].kind == Kind::Enter) {
            Enter(&context);
        } else {
            Exit(&context);
        }
        index += 1;
    }

    return context.trees[0].tree;
}

} // namespace markdown
