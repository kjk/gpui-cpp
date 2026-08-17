/* Themed tabs — crates/ui/src/tab */

#pragma once

#include "component/Common.h"

namespace gpui {

namespace component {

struct Tabs {
    Arena* a = nullptr;
    Str labels[8] = {};
    int n = 0;
    int selected = 0;
    Func1<int> onChange;

    static Tabs* New(Arena* a);
    Tabs* Tab(Str label);
    Tabs* Selected(int i);
    Tabs* OnChange(Func1<int> fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
