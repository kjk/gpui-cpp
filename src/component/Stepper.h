/* Themed stepper — crates/ui/src/stepper */

#include "component/Common.h"

namespace gpui {

namespace component {

struct Stepper {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str steps[8] = {};
    IconName icons[8] = {};
    int n = 0;
    int current = 0;
    // A horizontal stepper spreads its steps across the width it is given,
    // with the connector lines taking the space between them.
    float width = kFill;
    Listener onChange;

    static Stepper* New(Ctx* cx);
    Stepper* Step(Str s);
    Stepper* Step(Str s, IconName icon);
    Stepper* Current(int i);
    Stepper* W(float px);
    Stepper* OnChange(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
