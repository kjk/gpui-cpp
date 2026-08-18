/* Themed list — crates/ui/src/list */

#include "component/Common.h"

namespace gpui {

namespace component {

struct List {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str items[32] = {};
    int n = 0;
    int selected = -1;
    Listener onSelect;

    static List* New(Ctx* cx);
    List* Item(Str s);
    List* Selected(int i);
    List* OnSelect(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
