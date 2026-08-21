/* HTML into the same block tree markdown produces —
   crates/ui/src/text/format/html.rs

   Rust hands the source to html5ever, walks the RcDom and folds it into the
   BlockNode tree node.rs renders. There is no html5ever here and no room for
   one (hard rule 3), so this is a tokenizer and a stack of open elements that
   fold the same tags into ui/text.h's MdNode — the tree src/markdown already
   builds. One renderer walks both, which is exactly how Rust arranges it.

   The subset is Rust's: the block elements it lists, the inline marks it
   understands (b/strong, i/em, code, u, s/del, mark, a), tables with their
   alignment, and <img> reduced to its alt text. `style` and `script` are
   dropped, as they are there. */

#include "ui/text.h"

namespace gpui {

namespace component {

// A whole HTML document. The returned node is MdKind::Doc, the same root
// MdParse returns.
MdNode* HtmlParse(Arena* a, Str source);

// A fragment, appended to `parent`'s children. This is what a raw HTML block
// inside markdown goes through — Rust's markdown_ext.rs hands mdast::Html to
// format::html the same way.
void HtmlParseInto(Arena* a, MdNode* parent, Str source);

// One inline tag as it appears inside a markdown paragraph: `<b>`, `</b>`,
// `<a href="..">`, `<br>`, `<img alt="..">`. The markdown parser hands those
// over as an mdast Html node and text.cpp turns them into marks with this.
struct HtmlInlineTag {
    // The MdMark bits the tag sets, or clears when `close`.
    uint8_t mark = 0;
    bool close = false;
    bool known = false;
    // <br>: a hard break rather than a mark.
    bool isBreak = false;
    // <a href>, and an <img>'s source, alt text and given size.
    Str href = {};
    Str alt = {};
    Str src = {};
    float width = 0;
    float height = 0;
    bool isImage = false;
};

HtmlInlineTag HtmlParseInlineTag(Arena* a, Str tag);

// The value of one attribute, entity-decoded, or an empty Str when the tag
// does not carry it.
Str HtmlAttrValue(Arena* a, Str attrs, const char* name);

} // namespace component
} // namespace gpui
