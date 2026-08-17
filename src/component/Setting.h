/* Themed settings — crates/ui/src/setting */

#pragma once

#include "component/Common.h"

namespace gpui {

namespace component {

struct SettingItem {
    Str label = {};
    El* control = nullptr;
};

struct Setting {
    Arena* a = nullptr;
    Str title = {};
    SettingItem items[12] = {};
    int n = 0;

    static Setting* New(Arena* a, Str title);
    Setting* Item(Str label, El* control);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
