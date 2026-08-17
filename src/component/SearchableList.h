/* Themed searchable list — crates/ui/src/searchable_list */

#include "component/List.h"

namespace gpui {

namespace component {

struct SearchableList {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    LineInput* query = nullptr;
    Str items[32] = {};
    int n = 0;
    Func1<int> onSelect;

    static SearchableList* New(Ctx* cx, LineInput* query);
    SearchableList* Item(Str s);
    SearchableList* OnSelect(Func1<int> fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
