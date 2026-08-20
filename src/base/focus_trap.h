/* Focus trap — crates/base/src/focus_trap.rs

   A container that keeps Tab inside it: a dialog, a sheet, an alert. Rust
   registers the container's focus handle in a global FocusTrapManager, and
   Root's Tab action asks `active_focus_trap` whether the focused element is
   inside one before it moves focus. The runtime here already carries the
   trap on the element (`El::TrapId`) and FocusNext already refuses to leave
   one, so this is the other half: which trap has focus, and how focus gets
   into a trap that has just opened.

   A trap id is a hash of a name, the way a click id is, so the same container
   comes back with the same trap across frames. */

#include "gpui/gpui.h"

namespace gpui {

// The name a container traps under. Same hash as a click id — a dialog can
// pass its own id and get a trap of its own without inventing a number.
int FocusTrapId(Str name);

// FocusTrapManager::find_active_trap: the trap the focused element sits in,
// or 0 when focus is outside every trap. A trap with nothing focusable in it
// is never active, which is what makes an empty container harmless.
int FocusTrapActive(const Window* win);

// The trap `focusId` belongs to, or 0.
int FocusTrapOf(const Window* win, int focusId);

// Root::on_action_tab / on_action_tab_prev: move focus, staying inside the
// active trap when there is one. Answers the id now focused.
int FocusTrapTab(Window* win, bool backward);

// Move focus to the first focusable inside `trapId` (the last, backwards).
// Answers whether focus moved; a trap with nothing focusable in it leaves
// focus where it was.
bool FocusTrapEnter(Window* win, int trapId, bool backward = false);

// A container says it is open this frame. Rust does this by tracking focus on
// the trap container as it renders; here the element tree is built before the
// focusables are known, so the request is armed during the build and settled
// after the frame's FocusCollect.
void FocusTrapArm(Window* win, int trapId);

// Settle what FocusTrapArm asked for: if the armed trap does not already hold
// focus, focus its first element. Called once a frame, after FocusCollect.
void FocusTrapApplyPending(Window* win);

} // namespace gpui
