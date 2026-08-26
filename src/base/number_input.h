/* Unstyled number input — crates/base/src/number_input.rs */

#include "gpui/gpui.h"

namespace gpui {

// Rust's StepAction: which way a step goes.
enum class StepAction : uint8_t {
    Decrement,
    Increment
};

// step_value: step a numeric *string*, preserving the decimal precision the
// text already had and clamping into the range.
//
// Working on the text rather than a double is the point. "1.50" stepped by
// one is "2.50", not "2.5", because the field is what the reader is looking
// at; the digit count is the widest of the value's, the step's, and — where a
// bound is what the result landed on — that bound's.
//
// Answers false where Rust answers None: nothing to write, either because the
// text is not a number the step can move or because the clamp put the result
// back where it started. `hasMin` / `hasMax` are Rust's Option.
bool NumberStepValue(Str value, StepAction action, double step, bool hasMin,
                     double min, bool hasMax, double max, char* out,
                     int outCap);
// The same strict parser NumberInput uses before it steps, exposed so the
// themed wrapper can project its numeric accessibility value.
bool NumberParseValue(Str value, double* out);

// The key table: crates/base/src/number_input.rs binds up to Increment and
// down to Decrement in the "NumberInput" context. Answers false for anything
// else, which is a key the field itself should keep.
bool NumberStepForKey(int key, StepAction* out);

// The frame around the editor and its two step buttons. Identity only; the
// caller owns the parts, and the step buttons are Buttons that decline focus
// so a press on one leaves the editor focused. `InputBase::new(("number-
// input", state.entity_id()))`: the frame names itself so that "decrement"
// and "increment" under it are one spinbutton's and not another's — the id
// the caller passes is what stands in for the state's entity id here.
struct NumberInput {
    static El* New(Ctx* cx, Str id);
};
} // namespace gpui
