/* Unstyled input / textarea / editor — crates/base/src/input */

#include "gpui/Gpui.h"

namespace gpui {

struct InputBase {
    static El* New(Ctx* cx, Str id, int clickId = 0);
};

struct Input {
    static El* New(Ctx* cx, LineInput* state);
};

struct Textarea {
    static El* New(Ctx* cx, const char* text, bool caret = false);
};

struct Editor {
    static El* New(Ctx* cx, const char* text);
    static El* New(Ctx* cx, const char* text, int cursor, bool caret);
};
} // namespace gpui
