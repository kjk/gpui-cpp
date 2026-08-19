/* Unstyled popover — crates/base/src/popover.rs */

#include "gpui/gpui.h"

namespace gpui {

// Rust's PopoverState: the open lifecycle belongs to the popover, not to
// whichever page drew it. Uncontrolled by default — the trigger toggles it —
// and a caller that wants to drive it calls PopoverSetOpen every frame.
//
// Rust's state also parks the previously focused handle and restores it on
// close, and holds a dismiss subscription while open. Focus restoration needs
// a focus handle per popover, which the runtime has no room for yet; the
// subscription is the window's single unhandled-click slot, so it is the
// caller's for now — see the note on PopoverDismissOnOutsideClick.
struct PopoverState {
    bool open = false;
    // Whether default_open has been applied yet. Rust passes it to
    // PopoverState::new, which only runs the first time the key is seen.
    bool seeded = false;
};

bool PopoverIsOpen(Ctx* cx, Entity<PopoverState> state);
void PopoverSetOpen(Ctx* cx, Entity<PopoverState> state, bool open);
// `button` is which press this popover answers to: one element hears every
// press it is under, so the filter Rust does at registration is done here.
void PopoverToggle(PopoverState* self, Ctx* cx, const MouseDownEvent* ev,
                   intptr_t button);
void PopoverDismiss(PopoverState* self, Ctx* cx, const ClickEvent* ev);

// The popover. The trigger takes the *press*, not the click, and on whichever
// button was asked for — `Popover::mouse_button`, which is what a right-click
// popover is. `overlayClosable` is Rust's on_mouse_down_out.
struct Popover {
    Arena* a = nullptr;
    El* root = nullptr;
    Entity<PopoverState> state = {};
    MouseButton button = MouseButton::Left;

    static Popover* New(Ctx* cx, Str id, Entity<PopoverState> state = {},
                        MouseButton button = MouseButton::Left);
    Popover* Trigger(El* trigger);
    Popover* Content(El* content);
    El* IntoEl();
};
} // namespace gpui
