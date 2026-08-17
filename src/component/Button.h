/* Themed button — crates/ui/src/button/button.rs */

#pragma once

#include "component/Common.h"

namespace gpui {

namespace component {

enum class ButtonVariant : u8 {
    Default,
    Primary,
    Secondary,
    Danger,
    Info,
    Success,
    Warning,
    Ghost,
    Link,
    Text
};

struct Button {
    Arena* a = nullptr;
    Str id = {};
    Str label = {};
    IconName icon = IconName::None;
    ButtonVariant variant = ButtonVariant::Default;
    UiSize size = UiSize::Medium;
    bool outline = false;
    bool disabled = false;
    bool loading = false;
    bool compact = false;
    Str tooltip = {};
    Func0 onClick;

    static Button* New(Arena* a, Str id);
    Button* Label(Str s);
    Button* Icon(IconName n);
    Button* Primary();
    Button* Secondary();
    Button* Danger();
    Button* Warning();
    Button* Success();
    Button* Info();
    Button* Ghost();
    Button* Link();
    Button* Text();
    Button* Outline();
    Button* Compact();
    Button* Loading(bool v);
    Button* Disabled(bool v);
    Button* WithSize(UiSize s);
    Button* Tooltip(Str s);
    Button* OnClick(Func0 fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
