/* Unstyled button primitive — port of crates/base/src/button.rs.
   Owns click + focus. Layout, color, and typography stay with the caller. */

#include "gpui/Gpui.h"

namespace gpui {

struct Button {
    // id is the stable element id (Rust ElementId). clickId is the C++
    // hit/focus token (Rust on_click). Pass 0 for a non-interactive shell.
    static El* New(Ctx* cx, Str id, int clickId = 0);
};
} // namespace gpui
