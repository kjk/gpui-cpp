/* Unstyled dialog — crates/base/src/dialog.rs */

#include "gpui/gpui.h"

namespace gpui {

// Why a dialog's open state changed. Rust reports it alongside the new value
// so a caller can tell a confirm from a dismissal — the same close, but not
// the same outcome.
enum class DialogChangeReason : uint8_t {
    TriggerPress,
    BackdropPress,
    Cancel,
    Confirm,
    Imperative
};

// What a keystroke asks a dialog to do. Rust binds escape to Cancel and enter
// to Confirm, and hangs the key context off `keyboard`, so a dialog with
// keyboard off answers to neither.
enum class DialogAction : uint8_t {
    None,
    Cancel,
    Confirm
};

// `keyboard` is what `close_on_escape` sets: Rust's setter assigns the
// keyboard flag, so turning escape off takes Enter with it — the two share one
// key context.
DialogAction DialogActionForKey(int key, bool keyboard);

// Whether a press on the backdrop dismisses. Rust checks four things in
// on_any_mouse_down: the press is below the region reserved at the top — a
// drag on the title bar is not a dismissal — the button is the left one, the
// dialog is `overlay_closable`, and it is the topmost of a stack, so a press
// only ever closes the one on top. `overlayClosable` is what
// `close_on_backdrop_press` sets.
bool DialogBackdropCloses(bool overlayClosable, bool topmost,
                          MouseButton button, float pressY,
                          float dismissBelowY);

struct DialogBackdrop {
    static El* New(Ctx* cx);
};
struct DialogPopup {
    static El* New(Ctx* cx);
};
struct DialogTitle {
    static El* New(Ctx* cx);
};
struct DialogDescription {
    static El* New(Ctx* cx);
};
struct DialogClose {
    static El* New(Ctx* cx, int clickId = 0);
};

// The trigger takes the press, not the click, and stops it there — Rust's
// DialogTrigger is an on_mouse_down with cx.stop_propagation().
struct DialogTrigger {
    static El* New(Ctx* cx, Listener onOpen = {});
};

struct Dialog {
    El* root = nullptr;

    static Dialog* New(Ctx* cx);
    Dialog* Backdrop(El* backdrop);
    Dialog* Popup(El* popup);
    El* IntoEl();
};
} // namespace gpui
