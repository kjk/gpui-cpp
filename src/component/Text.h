/* Themed markdown view — crates/ui/src/text.

   Rust parses with the `markdown` crate into an mdast, converts that into its
   own BlockNode tree (text/node.rs) and renders the tree. There is no
   CommonMark parser to lean on here and no third-party C++ allowed, so this
   walks the source a line at a time and emits elements directly. It covers
   the subset the examples use: headings, horizontal rules, paragraphs, bullet
   and ordered lists, pipe tables, and **bold** / *italic* inline.

   This is the one markdown renderer in the tree. Rust has exactly one too —
   everything that shows markdown goes through TextView — so a page that needs
   it should come here rather than grow another parser. */

#include "component/Common.h"

namespace gpui {

namespace component {

struct TextView {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str source = {};
    // TextViewStyle::heading_base_font_size. Heading sizes are multiples of
    // it: node.rs 2258 has h1 2.0, h2 1.5, h3 1.25, h4 1.125, h5 and h6 1.0.
    float baseFont = 14;
    // Whether the text can be dragged over. Rust's TextView is selectable
    // through its own selection machinery; here it is El::Selectable.
    bool selectable = false;
    // A pipe table's column width. Rust measures the content and distributes
    // the space; this tree lays tables out on fixed columns.
    float tableColW = 140;

    static TextView* New(Ctx* cx, Str source);
    TextView* Font(float px);
    TextView* Selectable(bool on = true);
    TextView* TableColumnWidth(float px);
    El* IntoEl();

  private:
    El* ListRow(Str marker, Str text);
    // Consumes the run of table lines starting at `line`; returns where the
    // source continues.
    const char* Table(El* col, const char* line, const char* end);
};

} // namespace component
} // namespace gpui
