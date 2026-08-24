/* Unstyled input / textarea / editor — crates/base/src/input.

   The state engine is InputState in Gpui.h; this is element.rs, the half that
   draws it. */

#include "gpui/gpui.h"

namespace gpui {

struct InputBase {
    static El* New(Ctx* cx, Str id, int clickId = 0);
};

// gpui_base::input::InputEditorStyle. The base draws the text, the selection
// and the caret; what they look like is pushed in by the themed layer above
// it, the way Rust calls state.set_editor_style(...) before rendering.
struct InputEditorStyle {
    Rgba foreground = Rgb(0x17, 0x17, 0x17);
    Rgba mutedForeground = Rgb(0x73, 0x73, 0x73);
    Rgba caret = Rgb(0x17, 0x17, 0x17);
    Rgba selection = Rgba8(0x6b, 0xb3, 0xf0, 90);
    float fontSize = 12;
    // Editor::font_family(cx.theme().mono_font_family): a code editor draws
    // its rows, and its gutter, in the theme's monospace family.
    bool mono = false;
    // A masked field draws one bullet per character, and text_center /
    // text_right move the run inside the field. Both also live on the state;
    // either one turning it on is enough.
    bool mask = false;
    int align = 0;
    // The highlighted runs over the whole document, in order, as UTF-8
    // offsets into it: a syntax highlighter's captures and an editor's
    // TextDecorations both arrive this way. The rows slice what falls inside
    // them out of it, so a run may span more than one.
    const TextSpan* spans = nullptr;
    int nSpans = 0;
    // The search matches over the whole document, in order, as UTF-8 offsets
    // — `search_session.matcher.matched_ranges()`. The rows slice what falls
    // inside them out of it, the way they do the highlighted runs. Empty
    // while the find bar is closed, which is when Rust builds no paths.
    const Selection* matches = nullptr;
    int nMatches = 0;
    // Which of them is the one the panel is on, painted in its own colour so
    // it stands out from the rest. -1 for none.
    int currentMatch = -1;
    // Spelled out rather than left at `{}`: an Rgba defaults to opaque, so a
    // style that names none of these would paint an opaque black wash over
    // the caret's row -- which is what an editor built without `.active_line`
    // did.
    Rgba matchBg = {0, 0, 0, 0};
    Rgba currentMatchBg = {0, 0, 0, 0};
    // The editor's own surface, which an inline suggestion's second line and
    // beyond is drawn on: it covers the row under it rather than pushing it
    // down. Transparent means the rows show through, which is what a field
    // that never suggests anything gets.
    Rgba background = {0, 0, 0, 0};
    // The colour a symbol takes while the shortcut modifier is held over it
    // and the definition provider has somewhere to go. Rust reads `link_text`
    // out of the *highlight* theme; this tree's scanner palette has no such
    // entry, so the themed layer passes the UI theme's link colour.
    Rgba linkText = {0, 0, 0, 0};
    // EditorStyle::diagnostics: what each severity underlines in. All four
    // transparent is an editor that draws none, which is every field that is
    // not a code editor.
    DiagnosticColors diagnostics = {};
    // Editor's active-line wash and its indent guides. Alpha 0 and 0 are off,
    // which is what a plain textarea wants.
    Rgba activeLine = {0, 0, 0, 0};
    Rgba indentGuide = {0, 0, 0, 0};
    // How many columns an indent guide stands every, which is the language's
    // tab size.
    int indentWidth = 4;
};

struct Input {
    static El* New(Ctx* cx, InputState* state);
    static El* New(Ctx* cx, InputState* state, const InputEditorStyle& style);
};

struct Textarea {
    static El* New(Ctx* cx, InputState* state);
    static El* New(Ctx* cx, InputState* state, const InputEditorStyle& style,
                   bool lineNumbers = false);
};

struct Editor {
    static El* New(Ctx* cx, InputState* state);
    static El* New(Ctx* cx, InputState* state, const InputEditorStyle& style);
};
} // namespace gpui
