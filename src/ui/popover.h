/* Themed popover — crates/ui/src/popover.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

struct Popover {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    El* trigger = nullptr;
    El* content = nullptr;
    // Set only by Open(). Without it the popover keeps its own state and the
    // trigger's press toggles it, which is Rust's uncontrolled default;
    // Open() is Rust's `.open(Some(b))`.
    bool controlled = false;
    bool open = false;
    bool defaultOpen = false;
    // Popover::mouse_button. A right-button popover is a context menu.
    MouseButton button = MouseButton::Left;

    static Popover* New(Ctx* cx);
    static Popover* New(Ctx* cx, Str id);
    Popover* Trigger(El* e);
    Popover* Content(El* e);
    Popover* Open(bool v);
    Popover* DefaultOpen(bool v);
    Popover* Button(MouseButton b);
    El* IntoEl();
};

// Whether the popover of this id is showing, for a page that has to know
// before it builds the content.
bool PopoverOpen(Ctx* cx, Str id);

} // namespace component
} // namespace gpui
