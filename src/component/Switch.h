/* Themed switch — crates/ui/src/switch.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

struct Switch {
    Arena* a = nullptr;
    Str id = {};
    Str label = {};
    bool checked = false;
    bool disabled = false;
    Rgba color = {};
    bool hasColor = false;
    Func1<bool> onClick;

    static Switch* New(Arena* a, Str id);
    Switch* Label(Str s);
    Switch* Checked(bool v);
    Switch* Disabled(bool v);
    Switch* Color(Rgba c);
    Switch* OnClick(Func1<bool> fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
