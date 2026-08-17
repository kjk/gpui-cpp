/* Themed stepper — crates/ui/src/stepper */

#pragma once

#include "component/Common.h"

namespace component {

struct Stepper {
    Arena* a = nullptr;
    Str steps[8] = {};
    int n = 0;
    int current = 0;
    Func1<int> onChange;

    static Stepper* New(Arena* a);
    Stepper* Step(Str s);
    Stepper* Current(int i);
    Stepper* OnChange(Func1<int> fn);
    El* IntoEl();
};

} // namespace component
