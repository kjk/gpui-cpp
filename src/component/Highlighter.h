/* Themed highlighter façade — crates/ui/src/highlighter
   Syntax highlighting uses the simple keyword path from the showcase editor. */

#include "component/Common.h"

namespace gpui {

namespace component {

struct Highlighter {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    const char* text = nullptr;

    static Highlighter* New(Ctx* cx, const char* text);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
