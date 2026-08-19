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

// The frame around the editor and its two step buttons. Identity only; the
// caller owns the parts, and the step buttons are Buttons that decline focus
// so a press on one leaves the editor focused.
struct NumberInput {
    static El* New(Ctx* cx);
};
} // namespace gpui
