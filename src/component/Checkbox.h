/* Themed checkbox — crates/ui/src/checkbox.rs */

#pragma once

#include "component/Common.h"

namespace gpui {

namespace component {

struct Checkbox {
    Arena* a = nullptr;
    Str id = {};
    Str label = {};
    bool checked = false;
    bool disabled = false;
    UiSize size = UiSize::Medium;
    Str tooltip = {};
    Func1<bool> onClick;

    static Checkbox* New(Arena* a, Str id);
    Checkbox* Label(Str s);
    Checkbox* Checked(bool v);
    Checkbox* Disabled(bool v);
    Checkbox* WithSize(UiSize s);
    Checkbox* Tooltip(Str s);
    Checkbox* OnClick(Func1<bool> fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
