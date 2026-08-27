#ifndef GPUI_SRC_UI_SWITCH_H_
#define GPUI_SRC_UI_SWITCH_H_
/* Themed switch — crates/ui/src/switch.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

struct Switch {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Str label = {};
    bool checked = false;
    bool disabled = false;
    UiSize size = UiSize::Medium;
    Rgba color = {};
    bool hasColor = false;
    Listener onClick;

    static Switch* New(Ctx* cx, Str id);
    Switch* Label(Str s);
    Switch* Checked(bool v);
    Switch* Disabled(bool v);
    Switch* WithSize(UiSize s);
    Switch* Color(Rgba c);
    Switch* OnClick(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
#endif // GPUI_SRC_UI_SWITCH_H_
