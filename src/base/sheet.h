/* Unstyled sheet — crates/base/src/sheet.rs */

#include "gpui/gpui.h"

namespace gpui {

// What a press on a sheet's overlay comes to. Rust's on_any_mouse_down has
// three outcomes and the middle one is easy to miss: an overlay that is
// interactive but not closable still stops the press, so it never reaches the
// page behind — that is what an overlay is for.
enum class SheetOverlayPress : uint8_t {
    // The overlay does not take presses, or this one landed above the cutoff.
    Ignore,
    // Taken and stopped, but the sheet stays.
    Swallow,
    // Taken, stopped, and the sheet closes.
    Close
};

// `hasDismissBefore` is Rust's Option on dismiss_before_y: a press above that
// line is not the overlay's, which is how a sheet leaves a title bar usable
// while it is open.
SheetOverlayPress SheetOverlayPressAction(bool overlayInteractive,
                                          bool overlayClosable,
                                          MouseButton button, float pressY,
                                          bool hasDismissBefore,
                                          float dismissBeforeY);

// Escape closes a sheet. Rust calls cx.propagate() first and closes anyway, so
// the key is not consumed — whatever encloses the sheet still sees it.
bool SheetClosesOnKey(int key);

struct Sheet {
    Ctx* cx = nullptr;
    El* root = nullptr;
    // focus_trap("sheet"): the surface holds Tab while the sheet is open.
    Str trap = {};

    static Sheet* New(Ctx* cx);
    Sheet* Trap(Str name);
    Sheet* Overlay(El* overlay);
    Sheet* Surface(El* surface);
    El* IntoEl();
};
} // namespace gpui
