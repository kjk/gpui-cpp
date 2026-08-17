/* Themed list — crates/ui/src/list */

#include "component/Common.h"

namespace gpui {

namespace component {

struct List {
    Arena* a = nullptr;
    Str items[32] = {};
    int n = 0;
    int selected = -1;
    Func1<int> onSelect;

    static List* New(Arena* a);
    List* Item(Str s);
    List* Selected(int i);
    List* OnSelect(Func1<int> fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
