/* Themed checkbox — crates/ui/src/checkbox.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

struct Checkbox {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Str label = {};
    Str hint = {};
    bool checked = false;
    bool disabled = false;
    UiSize size = UiSize::Medium;
    Str tooltip = {};
    float w = 0;
    Func1<bool> onClick;

    static Checkbox* New(Ctx* cx, Str id);
    Checkbox* Label(Str s);
    Checkbox* Hint(Str s);
    Checkbox* Checked(bool v);
    Checkbox* Disabled(bool v);
    Checkbox* WithSize(UiSize s);
    Checkbox* W(float v);
    Checkbox* Tooltip(Str s);
    Checkbox* OnClick(Func1<bool> fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
