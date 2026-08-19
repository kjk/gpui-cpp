/* Unstyled input / textarea / editor — crates/base/src/input.

   The state engine is InputState in Gpui.h; this is element.rs, the half that
   draws it. */

#include "gpui/gpui.h"

namespace gpui {

struct InputBase {
    static El* New(Ctx* cx, Str id, int clickId = 0);
};

// gpui_base::input::InputEditorStyle. The base draws the text, the selection
// and the caret; what they look like is pushed in by the themed layer above
// it, the way Rust calls state.set_editor_style(...) before rendering.
struct InputEditorStyle {
    Rgba foreground = Rgb(0x17, 0x17, 0x17);
    Rgba mutedForeground = Rgb(0x73, 0x73, 0x73);
    Rgba caret = Rgb(0x17, 0x17, 0x17);
    Rgba selection = Rgba8(0x6b, 0xb3, 0xf0, 90);
    float fontSize = 12;
    // A masked field draws one bullet per character, and text_center /
    // text_right move the run inside the field. Both also live on the state;
    // either one turning it on is enough.
    bool mask = false;
    int align = 0;
};

struct Input {
    static El* New(Ctx* cx, InputState* state);
    static El* New(Ctx* cx, InputState* state, const InputEditorStyle& style);
};

struct Textarea {
    static El* New(Ctx* cx, InputState* state);
    static El* New(Ctx* cx, InputState* state, const InputEditorStyle& style,
                   bool lineNumbers = false);
};

struct Editor {
    static El* New(Ctx* cx, InputState* state);
    static El* New(Ctx* cx, InputState* state, const InputEditorStyle& style);
};
} // namespace gpui
