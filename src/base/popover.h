/* Unstyled popover — crates/base/src/popover.rs */

#include "gpui/gpui.h"
#include "base/popup.h"

namespace gpui {

// Rust's PopoverState: the open lifecycle belongs to the popover, not to
// whichever page drew it. Uncontrolled by default — the trigger toggles it —
// and a caller that wants to drive it calls PopoverSetOpen every frame.
//
// Rust's state also parks the previously focused handle and restores it on
// close; so does this one. The dismiss subscription is the window's single
// unhandled-click slot, so that half is still the caller's — see the note on
// PopoverDismissOnOutsideClick.
struct PopoverState {
    bool open = false;
    // Whether default_open has been applied yet. Rust passes it to
    // PopoverState::new, which only runs the first time the key is seen.
    bool seeded = false;
    // The popover's own focus handle — Rust hangs it off the content — and
    // `tracked_focus_handle`, what it focuses instead, which is how a select
    // hands focus to the query field inside it. `Popover::New` fills the
    // first in from the id. Focus that has since moved on to something
    // *inside* the content does not read as the popover's, so a popover the
    // reader tabbed into is left alone on close rather than pulled back;
    // Rust's handle answers for its whole subtree.
    FocusHandle focus = {};
    FocusHandle trackedFocus = {};
    // previous_focus_handle: where focus was when it opened, put back on
    // close if the popover still holds it.
    FocusHandle previousFocus = {};
};

// Open or close, doing what Rust's `toggle_open` does around it: the focus
// goes into the popover (or into whatever it tracks) and comes back out to
// where it was. A widget that is a popover in all but name — a select, a
// dropdown menu — can call this with a state of its own.
void PopoverSetOpenFocused(PopoverState* s, Ctx* cx, bool open);

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
    Ctx* cx = nullptr;
    El* root = nullptr;
    Entity<PopoverState> state = {};
    FocusHandle focus = {};
    MouseButton button = MouseButton::Left;
    // Popover::anchor, which is the corner of the trigger the content hangs
    // off — Top* below it, Bottom* above it, and Left/Center/Right the edge
    // the two line up on.
    PopupAnchor anchor = PopupAnchor::TopLeft;

    static Popover* New(Ctx* cx, Str id, Entity<PopoverState> state = {},
                        MouseButton button = MouseButton::Left);
    Popover* Anchor(PopupAnchor v);
    // Popover::tracked_focus_handle: what takes focus when it opens, instead
    // of the popover itself.
    Popover* TrackedFocus(FocusHandle tracked);
    Popover* Trigger(El* trigger);
    Popover* Content(El* content);
    El* IntoEl();
};
} // namespace gpui
