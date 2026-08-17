/* Unstyled input / textarea / editor — crates/base/src/input */

#pragma once

#include "gpui/Gpui.h"

namespace gpui {

struct InputBase {
    static El* New(Arena* a, Str id, int clickId = 0);
};

struct Input {
    static El* New(Arena* a, LineInput* state);
};

struct Textarea {
    static El* New(Arena* a, const char* text, bool caret = false);
};

struct Editor {
    static El* New(Arena* a, const char* text);
    static El* New(Arena* a, const char* text, int cursor, bool caret);
};
} // namespace gpui
