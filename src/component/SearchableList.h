/* Themed searchable list — crates/ui/src/searchable_list */

#pragma once

#include "component/List.h"

namespace gpui {

namespace component {

struct SearchableList {
    Arena* a = nullptr;
    LineInput* query = nullptr;
    Str items[32] = {};
    int n = 0;
    Func1<int> onSelect;

    static SearchableList* New(Arena* a, LineInput* query);
    SearchableList* Item(Str s);
    SearchableList* OnSelect(Func1<int> fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
