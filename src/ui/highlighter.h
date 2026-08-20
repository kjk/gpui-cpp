/* Themed highlighter façade — crates/ui/src/highlighter
   Syntax highlighting uses the simple keyword path from the showcase editor. */

#include "ui/sizing.h"
#include "ui/syntax.h"

namespace gpui {

namespace component {

struct Highlighter {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    // EditorState: the same engine again, with InputKind::Editor.
    InputState* state = nullptr;
    // The box the rows scroll inside. 0 lets the editor be as tall as its
    // content, which is what an editor inside something else that scrolls
    // wants.
    float h = 0;
    // EditorState::language: what the rows are scanned as. None leaves them
    // in the editor's own colour.
    SyntaxLang lang = SyntaxLangNone;
    // create_decorations_collection: runs the caller wants painted over the
    // document, in document offsets and in order. They are laid over the
    // language's own captures.
    const TextSpan* decorations = nullptr;
    int nDecorations = 0;
    // The active-line wash and the indent guides, both off by default.
    bool activeLine = false;
    bool indentGuides = false;

    static Highlighter* New(Ctx* cx, InputState* state);
    static Highlighter* New(Ctx* cx, Str id, InputState* state);
    Highlighter* H(float v);
    Highlighter* Language(Str name);
    Highlighter* Decorations(const TextSpan* runs, int n);
    Highlighter* ActiveLine(bool v = true);
    Highlighter* IndentGuides(bool v = true);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
