/* Themed menu — crates/ui/src/menu */

#include "component/Common.h"

namespace gpui {

namespace component {

struct Menu {
    Arena* a = nullptr;
    Str items[8] = {};
    int n = 0;
    Func1<int> onClick;

    static Menu* New(Arena* a);
    Menu* Item(Str s);
    Menu* OnClick(Func1<int> fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
