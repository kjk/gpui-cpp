/* Themed combobox — crates/ui/src/combobox.rs */

#pragma once

#include "component/Select.h"

namespace gpui {

namespace component {

struct Combobox {
    Arena* a = nullptr;
    Str id = {};
    Str options[8] = {};
    int n = 0;
    Str selected = {};
    bool open = false;
    LineInput* query = nullptr;
    Func1<int> onChange;
    Func0 onToggle;

    static Combobox* New(Arena* a, Str id);
    Combobox* Option(Str s);
    Combobox* Selected(Str s);
    Combobox* Open(bool v);
    Combobox* Query(LineInput* q);
    Combobox* OnChange(Func1<int> fn);
    Combobox* OnToggle(Func0 fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
