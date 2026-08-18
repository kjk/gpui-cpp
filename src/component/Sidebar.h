/* Themed sidebar — crates/ui/src/sidebar */

#include "component/Common.h"

namespace gpui {

namespace component {

struct Sidebar {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str title = {};
    Str items[8] = {};
    int n = 0;
    int selected = 0;
    bool collapsed = false;
    Listener onSelect;

    static Sidebar* New(Ctx* cx);
    Sidebar* Title(Str s);
    Sidebar* Item(Str s);
    Sidebar* Selected(int i);
    Sidebar* Collapsed(bool v);
    Sidebar* OnSelect(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
