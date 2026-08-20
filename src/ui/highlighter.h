/* Themed highlighter façade — crates/ui/src/highlighter
   Syntax highlighting uses the simple keyword path from the showcase editor. */

#include "ui/sizing.h"

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

    static Highlighter* New(Ctx* cx, InputState* state);
    static Highlighter* New(Ctx* cx, Str id, InputState* state);
    Highlighter* H(float v);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
