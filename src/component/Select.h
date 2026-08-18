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
    Listener onChange;
    Listener onToggle;

    static Select* New(Ctx* cx, Str id);
    Select* Option(Str s);
    Select* Selected(int i);
    Select* Open(bool v);
    Select* OnChange(Listener fn);
    Select* OnToggle(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
