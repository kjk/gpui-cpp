/* src/lib.rs + src/configuration.rs — the public entry point.

   Part of the C++ port of markdown-rs 1.0.0 (see src/markdown/readme.md). */

#include "markdown/construct.h"

namespace markdown {

Constructs Constructs::Gfm() {
    Constructs constructs;
    constructs.gfmAutolinkLiteral = true;
    constructs.gfmFootnoteDefinition = true;
    constructs.gfmLabelStartFootnote = true;
    constructs.gfmStrikethrough = true;
    constructs.gfmTable = true;
    constructs.gfmTaskListItem = true;
    return constructs;
}

ParseOptions ParseOptions::Gfm() {
    ParseOptions options;
    options.constructs = Constructs::Gfm();
    return options;
}

Node* ToMdast(Arena* a, Str source, const ParseOptions& options) {
    ParseState parseState;
    parseState.a = a;
    // The parse's own working memory, thrown away whole below, so none of it
    // is left in the caller's arena.
    parseState.scratch = base::ArenaNew();
    parseState.options = &options;
    parseState.bytes = source;

    Vec<Event> events = Parse(&parseState);
    Node* tree = ToMdastCompile(events, &parseState);

    base::ArenaDelete(parseState.scratch);
    return tree;
}

} // namespace markdown
