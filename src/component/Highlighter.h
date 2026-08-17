/* Themed highlighter façade — crates/ui/src/highlighter
   Syntax highlighting uses the simple keyword path from the showcase editor. */

#pragma once

#include "component/Common.h"

namespace gpui {

namespace component {

struct Highlighter {
    Arena* a = nullptr;
    const char* text = nullptr;

    static Highlighter* New(Arena* a, const char* text);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
