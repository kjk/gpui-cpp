/* Unstyled button primitive — port of crates/base/src/button.rs.
   Owns click + focus. Layout, color, and typography stay with the caller. */

#pragma once

#include "gpui/Gpui.h"

struct Button {
    // id is the stable element id (Rust ElementId). clickId is the C++ hit/focus
    // token (Rust on_click). Pass 0 for a non-interactive shell.
    static El* New(Arena* a, Str id, int clickId = 0);
};
