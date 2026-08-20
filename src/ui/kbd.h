/* Themed kbd — crates/ui/src/kbd.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

// gpui::Keystroke: the modifiers a binding holds down and the key it ends on.
// `key` is the name GPUI uses — "c", "enter", "left", "pagedown" — lowercase,
// which is what `Kbd::format` matches on.
struct Keystroke {
    bool ctrl = false;
    bool alt = false;
    bool shift = false;
    // The platform key: Command on macOS, Windows everywhere else.
    bool platform = false;
    Str key = {};
};

// Kbd::format: how the platform spells a keystroke. macOS runs the modifiers
// together in ⌃⌥⇧⌘ order and uses its glyphs for the named keys; everything
// else spells them out and joins with "+", in Ctrl+Alt+Shift+Win order. A key
// with no name of its own is capitalised.
//
// Writes into `out` and answers how many bytes it wrote, not counting the
// terminator — which is always written when there is room for it.
int KbdFormat(Keystroke stroke, char* out, int cap);
// The same, into the frame arena.
Str KbdFormatStr(Ctx* cx, Keystroke stroke);

struct Kbd {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str stroke = {};
    bool appearance = true;
    bool outline = false;

    static Kbd* New(Ctx* cx, Str stroke);
    // The keystroke, spelled the way this platform spells it.
    static Kbd* New(Ctx* cx, Keystroke stroke);
    Kbd* Appearance(bool v);
    Kbd* Outline();
    El* IntoEl();
};

} // namespace component
} // namespace gpui
