/* Themed combobox — crates/ui/src/combobox.rs */

#include "component/Select.h"

namespace gpui {

namespace component {

struct Combobox {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Str options[12] = {};
    int n = 0;
    // The trigger shows the selection, or the placeholder when there is none.
    Str selected = {};
    Str placeholder = {};
    Str searchPlaceholder = {};
    // An optional icon before the title, as the icons story shows.
    IconName icon = IconName::None;
    float width = 280;
    bool open = false;
    LineInput* query = nullptr;
    Listener onChange;
    Listener onToggle;

    static Combobox* New(Ctx* cx, Str id);
    Combobox* Option(Str s);
    Combobox* Options(const char* const* items, int count);
    Combobox* Selected(Str s);
    Combobox* Placeholder(Str s);
    Combobox* SearchPlaceholder(Str s);
    Combobox* Icon(IconName n);
    Combobox* W(float v);
    Combobox* Open(bool v);
    Combobox* Query(LineInput* q);
    Combobox* OnChange(Listener fn);
    Combobox* OnToggle(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
