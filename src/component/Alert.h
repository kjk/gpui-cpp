/* Themed alert — crates/ui/src/alert.rs */

#pragma once

#include "component/Common.h"

namespace component {

enum class AlertVariant : u8 { Default, Info, Success, Warning, Error };

struct Alert {
    Arena* a = nullptr;
    Str id = {};
    AlertVariant variant = AlertVariant::Default;
    IconName icon = IconName::Info;
    Str title = {};
    Str message = {};
    UiSize size = UiSize::Medium;
    bool banner = false;
    bool visible = true;
    Func0 onClose;

    static Alert* New(Arena* a, Str id, Str message);
    static Alert* Info(Arena* a, Str id, Str message);
    static Alert* Success(Arena* a, Str id, Str message);
    static Alert* Warning(Arena* a, Str id, Str message);
    static Alert* Error(Arena* a, Str id, Str message);
    Alert* Title(Str s);
    Alert* Icon(IconName n);
    Alert* Banner();
    Alert* Visible(bool v);
    Alert* OnClose(Func0 fn);
    Alert* WithSize(UiSize s);
    El* IntoEl();
};

} // namespace component
