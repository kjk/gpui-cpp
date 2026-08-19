/* Themed settings — crates/ui/src/setting */

#include "ui/sizing.h"

namespace gpui {

namespace component {

struct SettingItem {
    Str label = {};
    El* control = nullptr;
};

struct Setting {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str title = {};
    SettingItem items[12] = {};
    int n = 0;

    static Setting* New(Ctx* cx, Str title);
    Setting* Item(Str label, El* control);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
