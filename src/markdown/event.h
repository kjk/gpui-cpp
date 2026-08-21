/* src/event.rs — the events a tokenize produces.

   A parse is a flat list of enter/exit events over the source bytes, and
   everything above it (the resolvers, to_mdast) reads that list. See
   src/markdown/readme.md for the file-for-file map. */

#ifndef GPUI_MARKDOWN_EVENT_H_
#define GPUI_MARKDOWN_EVENT_H_

#include "base.h"

namespace markdown {

using base::Str;
using base::Vec;

// event.rs Name. The MDX names are gone with the MDX constructs; see the
// readme.
enum class Name : uint8_t {
    AttentionSequence,
    Autolink,
    AutolinkEmail,
    AutolinkMarker,
    AutolinkProtocol,
    BlankLineEnding,
    BlockQuote,
    BlockQuoteMarker,
    BlockQuotePrefix,
    ByteOrderMark,
    CharacterEscape,
    CharacterEscapeMarker,
    CharacterEscapeValue,
    CharacterReference,
    CharacterReferenceMarker,
    CharacterReferenceMarkerHexadecimal,
    CharacterReferenceMarkerNumeric,
    CharacterReferenceMarkerSemi,
    CharacterReferenceValue,
    CodeFenced,
    CodeFencedFence,
    CodeFencedFenceInfo,
    CodeFencedFenceMeta,
    CodeFencedFenceSequence,
    CodeFlowChunk,
    CodeIndented,
    CodeText,
    CodeTextData,
    CodeTextSequence,
    Content,
    Data,
    Definition,
    DefinitionDestination,
    DefinitionDestinationLiteral,
    DefinitionDestinationLiteralMarker,
    DefinitionDestinationRaw,
    DefinitionDestinationString,
    DefinitionLabel,
    DefinitionLabelMarker,
    DefinitionLabelString,
    DefinitionMarker,
    DefinitionTitle,
    DefinitionTitleMarker,
    DefinitionTitleString,
    Emphasis,
    EmphasisSequence,
    EmphasisText,
    Frontmatter,
    FrontmatterChunk,
    FrontmatterFence,
    FrontmatterSequence,
    GfmAutolinkLiteralEmail,
    GfmAutolinkLiteralMailto,
    GfmAutolinkLiteralProtocol,
    GfmAutolinkLiteralWww,
    GfmAutolinkLiteralXmpp,
    GfmFootnoteCall,
    GfmFootnoteCallLabel,
    GfmFootnoteCallMarker,
    GfmFootnoteDefinition,
    GfmFootnoteDefinitionPrefix,
    GfmFootnoteDefinitionLabel,
    GfmFootnoteDefinitionLabelMarker,
    GfmFootnoteDefinitionLabelString,
    GfmFootnoteDefinitionMarker,
    GfmStrikethrough,
    GfmStrikethroughSequence,
    GfmStrikethroughText,
    GfmTable,
    GfmTableBody,
    GfmTableCell,
    GfmTableCellText,
    GfmTableCellDivider,
    GfmTableDelimiterRow,
    GfmTableDelimiterMarker,
    GfmTableDelimiterCell,
    GfmTableDelimiterCellValue,
    GfmTableDelimiterFiller,
    GfmTableHead,
    GfmTableRow,
    GfmTaskListItemCheck,
    GfmTaskListItemMarker,
    GfmTaskListItemValueChecked,
    GfmTaskListItemValueUnchecked,
    HardBreakEscape,
    HardBreakTrailing,
    HeadingAtx,
    HeadingAtxSequence,
    HeadingAtxText,
    HeadingSetext,
    HeadingSetextText,
    HeadingSetextUnderline,
    HeadingSetextUnderlineSequence,
    HtmlFlow,
    HtmlFlowData,
    HtmlText,
    HtmlTextData,
    Image,
    Label,
    LabelEnd,
    LabelImage,
    LabelImageMarker,
    LabelLink,
    LabelMarker,
    LabelText,
    LineEnding,
    Link,
    ListItem,
    ListItemMarker,
    ListItemPrefix,
    ListItemValue,
    ListOrdered,
    ListUnordered,
    MathFlow,
    MathFlowFence,
    MathFlowFenceMeta,
    MathFlowFenceSequence,
    MathFlowChunk,
    MathText,
    MathTextData,
    MathTextSequence,
    Paragraph,
    Reference,
    ReferenceMarker,
    ReferenceString,
    Resource,
    ResourceDestination,
    ResourceDestinationLiteral,
    ResourceDestinationLiteralMarker,
    ResourceDestinationRaw,
    ResourceDestinationString,
    ResourceMarker,
    ResourceTitle,
    ResourceTitleMarker,
    ResourceTitleString,
    SpaceOrTab,
    Strong,
    StrongSequence,
    StrongText,
    ThematicBreak,
    ThematicBreakSequence,
    LinePrefix,
};

// event.rs VOID_EVENTS, as a test rather than a list: the events that hold
// nothing between their enter and their exit.
bool IsVoidEvent(Name name);

// event.rs Content: which tokenizer a chunk is fed to later.
enum class ContentKind : uint8_t {
    Flow,
    Content,
    String,
    Text,
};

// event.rs Link. Rust's `Option<usize>` is -1 here.
struct Link {
    int32_t previous = -1;
    int32_t next = -1;
    ContentKind content = ContentKind::Flow;
};

// event.rs Point. `vs` is how far into a tab's virtual spaces we are.
struct Point {
    int32_t line = 1;
    int32_t column = 1;
    int32_t index = 0;
    int32_t vs = 0;
};

// event.rs Point::shift_to.
Point PointShiftTo(const Point& from, Str bytes, int32_t index);

enum class Kind : uint8_t {
    Enter,
    Exit,
};

// event.rs Event. `Option<Link>` is a flag plus the value, so an Event stays
// POD and a Vec<Event> stays memcpy-able.
struct Event {
    Kind kind = Kind::Enter;
    Name name = Name::Data;
    bool hasLink = false;
    Point point = {};
    Link link = {};
};

} // namespace markdown

#endif // GPUI_MARKDOWN_EVENT_H_
