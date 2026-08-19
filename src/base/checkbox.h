/* Unstyled checkbox — crates/base/src/checkbox.rs */

#include "gpui/gpui.h"

namespace gpui {

// The semantic value an unstyled checkbox exposes. Rust's CheckboxState.
enum class CheckboxState : uint8_t {
    Unchecked,
    Checked,
    Indeterminate
};

// What an activation produces: an indeterminate box becomes checked, a
// checked one clears. Rust's CheckboxState::activated.
CheckboxState CheckboxActivated(CheckboxState state);

// Rust's `Checkbox::new(id).state(..).disabled(..).on_change(..)`. The box
// owns identity, focus and activation; the caller owns every pixel of it.
// `onChange` is handed the state the activation produces, the way Rust hands
// its handler `next_state` — read it with a
// `void On(T*, Ctx*, const ClickEvent*, intptr_t next)` and compare against
// CheckboxState. A disabled box keeps its id, so it still hit-tests and
// hovers, but takes neither focus nor the click.
struct Checkbox {
    static El* New(Ctx* cx, Str id,
                   CheckboxState state = CheckboxState::Unchecked,
                   bool disabled = false, Listener onChange = {});
};

struct CheckboxIndicator {
    static El* New(Ctx* cx);
};
} // namespace gpui
