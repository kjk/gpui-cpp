/* Themed radio — crates/ui/src/radio.rs */

#pragma once

#include "component/Common.h"

namespace component {

struct Radio {
    Arena* a = nullptr;
    Str id = {};
    Str label = {};
    bool checked = false;
    bool disabled = false;
    Func1<bool> onClick;

    static Radio* New(Arena* a, Str id);
    Radio* Label(Str s);
    Radio* Checked(bool v);
    Radio* Disabled(bool v);
    Radio* OnClick(Func1<bool> fn);
    El* IntoEl();
};

} // namespace component
