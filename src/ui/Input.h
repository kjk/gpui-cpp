/* Unstyled input / textarea / editor — crates/base/src/input */

#include "gpui/Gpui.h"

namespace gpui {

struct InputBase {
    static El* New(Ctx* cx, Str id, int clickId = 0);
};

// gpui_base::input::InputEditorStyle. The base draws the text and the caret;
// what they look like is pushed in by the themed layer above it, the way Rust
// calls state.set_editor_style(...) before rendering.
struct InputEditorStyle {
    Rgba foreground = Rgb(0x17, 0x17, 0x17);
    Rgba mutedForeground = Rgb(0x73, 0x73, 0x73);
    Rgba caret = Rgb(0x17, 0x17, 0x17);
    float fontSize = 12;
};

struct Input {
    static El* New(Ctx* cx, LineInput* state);
    static El* New(Ctx* cx, LineInput* state, const InputEditorStyle& style);
};

struct Textarea {
    static El* New(Ctx* cx, const char* text, bool caret = false);
    static El* New(Ctx* cx, const char* text, const InputEditorStyle& style,
                   bool caret = false);
};

struct Editor {
    static El* New(Ctx* cx, const char* text);
    static El* New(Ctx* cx, const char* text, int cursor, bool caret);
};
} // namespace gpui
