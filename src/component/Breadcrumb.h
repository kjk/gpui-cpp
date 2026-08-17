/* Themed breadcrumb — crates/ui/src/breadcrumb.rs */

#pragma once

#include "component/Common.h"

namespace component {

struct Breadcrumb {
    Arena* a = nullptr;
    Str items[8] = {};
    int n = 0;
    int clickBase = 0;
    Func1<int> onClick;

    static Breadcrumb* New(Arena* a);
    Breadcrumb* Item(Str s);
    Breadcrumb* OnClick(Func1<int> fn);
    El* IntoEl();
};

} // namespace component
