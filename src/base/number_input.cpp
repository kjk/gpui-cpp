#include "base/number_input.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

namespace gpui {

// The digits after the '.' in a number written out as text.
static int FractionDigits(Str s) {
    if (!s.s) {
        return 0;
    }
    for (int i = 0; i < s.len; i++) {
        if (s.s[i] == '.') {
            return s.len - i - 1;
        }
    }
    return 0;
}

// Rust reads the fraction width off `step.to_string()` / `min.to_string()`,
// which print a float without trailing zeros. "%g" is the same shape, and it
// is only ever asked about the small, exact numbers a step and a bound are.
static int FractionDigitsOf(double v) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%g", v);
    return FractionDigits(Str(buf));
}

// The leading number in the text, or false if there is not one. Rust is
// `value.trim().parse::<f64>().ok()`, which refuses trailing junk, so a
// partial parse does not count.
static bool ParseNumber(Str value, double* out) {
    if (!value.s || value.len <= 0) {
        return false;
    }
    char buf[128];
    int n = value.len < (int)sizeof(buf) - 1 ? value.len : (int)sizeof(buf) - 1;
    memcpy(buf, value.s, (size_t)n);
    buf[n] = 0;
    char* end = nullptr;
    double v = strtod(buf, &end);
    if (end == buf) {
        return false;
    }
    while (*end == ' ' || *end == '\t') {
        end++;
    }
    if (*end != 0) {
        return false;
    }
    *out = v;
    return true;
}

bool NumberStepValue(Str value, StepAction action, double step, bool hasMin,
                     double min, bool hasMax, double max, char* out,
                     int outCap) {
    double current = 0;
    bool haveCurrent = ParseNumber(value, &current);
    double next = action == StepAction::Increment
                      ? (haveCurrent ? current : 0) + step
                      : (haveCurrent ? current : 0) - step;
    int digits = FractionDigits(value);
    int stepDigits = FractionDigitsOf(step);
    if (stepDigits > digits) {
        digits = stepDigits;
    }
    if (hasMin && next < min) {
        next = min;
        int d = FractionDigitsOf(min);
        if (d > digits) {
            digits = d;
        }
    }
    if (hasMax && next > max) {
        next = max;
        int d = FractionDigitsOf(max);
        if (d > digits) {
            digits = d;
        }
    }
    // A step that did not move the value is not a step: at a bound the field
    // keeps what it had rather than being rewritten to the same number.
    if (haveCurrent) {
        bool moved =
            action == StepAction::Increment ? next > current : next < current;
        if (!moved) {
            return false;
        }
    }
    snprintf(out, (size_t)outCap, "%.*f", digits, next);
    return true;
}

bool NumberStepForKey(int key, StepAction* out) {
    if (key == KeyUp) {
        *out = StepAction::Increment;
        return true;
    }
    if (key == KeyDown) {
        *out = StepAction::Decrement;
        return true;
    }
    return false;
}

El* NumberInput::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->Id(id);
}
} // namespace gpui
