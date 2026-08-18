/* Themed combobox — crates/ui/src/combobox.rs */

#include "component/Select.h"

namespace gpui {

namespace component {

struct Combobox {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Str options[8] = {};
    int n = 0;
    Str selected = {};
    bool open = false;
    LineInput* query = nullptr;
    Listener onChange;
    Listener onToggle;

    static Combobox* New(Ctx* cx, Str id);
    Combobox* Option(Str s);
    Combobox* Selected(Str s);
    Combobox* Open(bool v);
    Combobox* Query(LineInput* q);
    Combobox* OnChange(Listener fn);
    Combobox* OnToggle(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
