/* Themed virtual list — crates/ui/src/virtual_list.rs */

#pragma once

#include "component/Common.h"

namespace gpui {

namespace component {

struct VirtualList {
    Arena* a = nullptr;
    int count = 0;
    float rowH = 32;
    float viewH = 192;
    float scrollY = 0;
    Func1<int> onRenderRow; // not used; rows built here
    El* (*row)(Arena* a, int ix) = nullptr;

    static VirtualList* New(Arena* a, int count);
    VirtualList* RowH(float v);
    VirtualList* ViewH(float v);
    VirtualList* ScrollY(float v);
    VirtualList* Row(El* (*fn)(Arena*, int));
    El* IntoEl();
};

} // namespace component
} // namespace gpui
