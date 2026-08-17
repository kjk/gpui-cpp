/* Themed select — crates/ui/src/select.rs */

#pragma once

#include "component/Common.h"

namespace component {

struct Select {
    Arena* a = nullptr;
    Str id = {};
    Str options[8] = {};
    int n = 0;
    int selected = 0;
    bool open = false;
    Func1<int> onChange;
    Func0 onToggle;

    static Select* New(Arena* a, Str id);
    Select* Option(Str s);
    Select* Selected(int i);
    Select* Open(bool v);
    Select* OnChange(Func1<int> fn);
    Select* OnToggle(Func0 fn);
    El* IntoEl();
};

} // namespace component
