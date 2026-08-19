/* Themed highlighter façade — crates/ui/src/highlighter
   Syntax highlighting uses the simple keyword path from the showcase editor. */

#include "ui/sizing.h"

namespace gpui {

namespace component {

struct Highlighter {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    // EditorState: the same engine again, with InputKind::Editor.
    InputState* state = nullptr;

    static Highlighter* New(Ctx* cx, InputState* state);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
