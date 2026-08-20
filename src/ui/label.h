/* Themed label — crates/ui/src/label.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

struct Label {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str text = {};
    Str secondary = {};
    bool masked = false;
    bool semibold = false;
    float font = 14;
    // Label::highlights: the run to paint in the theme's blue. `prefixMatch`
    // is HighlightsMatch::Prefix, which only lights the start of the label;
    // otherwise every occurrence lights up (HighlightsMatch::Full).
    Str highlights = {};
    bool prefixMatch = false;
    // text_center / text_right, and line_height(rems(..)).
    int align = 0; // 0 start, 1 center, 2 end
    float lineHeight = 0;

    static Label* New(Ctx* cx, Str text);
    Label* Secondary(Str s);
    Label* Masked(bool v);
    Label* Semibold();
    Label* Font(float px);
    Label* Highlights(Str matched, bool prefix = false);
    Label* TextCenter();
    Label* TextRight();
    Label* LineHeight(float mult);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
