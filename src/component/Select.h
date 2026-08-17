/* Themed select — crates/ui/src/select.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

struct Select {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Str options[8] = {};
    int n = 0;
    int selected = 0;
    bool open = false;
    Func1<int> onChange;
    Func0 onToggle;

    static Select* New(Ctx* cx, Str id);
    Select* Option(Str s);
    Select* Selected(int i);
    Select* Open(bool v);
    Select* OnChange(Func1<int> fn);
    Select* OnToggle(Func0 fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
