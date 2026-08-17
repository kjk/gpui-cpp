/* Themed sidebar — crates/ui/src/sidebar */

#pragma once

#include "component/Common.h"

namespace component {

struct Sidebar {
    Arena* a = nullptr;
    Str title = {};
    Str items[8] = {};
    int n = 0;
    int selected = 0;
    bool collapsed = false;
    Func1<int> onSelect;

    static Sidebar* New(Arena* a);
    Sidebar* Title(Str s);
    Sidebar* Item(Str s);
    Sidebar* Selected(int i);
    Sidebar* Collapsed(bool v);
    Sidebar* OnSelect(Func1<int> fn);
    El* IntoEl();
};

} // namespace component
