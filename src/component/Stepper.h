/* Themed stepper — crates/ui/src/stepper */

#include "component/Common.h"

namespace gpui {

namespace component {

struct Stepper {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str steps[8] = {};
    int n = 0;
    int current = 0;
    Func1<int> onChange;

    static Stepper* New(Ctx* cx);
    Stepper* Step(Str s);
    Stepper* Current(int i);
    Stepper* OnChange(Func1<int> fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
