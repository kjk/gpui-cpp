/* Themed alert — crates/ui/src/alert.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

enum class AlertVariant : uint8_t {
    Default,
    Info,
    Success,
    Warning,
    Error
};

struct Alert {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    AlertVariant variant = AlertVariant::Default;
    IconName icon = IconName::Info;
    Str title = {};
    Str message = {};
    El* content = nullptr;
    UiSize size = UiSize::Medium;
    bool banner = false;
    bool visible = true;
    Func0 onClose;

    static Alert* New(Ctx* cx, Str id, Str message);
    static Alert* Info(Ctx* cx, Str id, Str message);
    static Alert* Success(Ctx* cx, Str id, Str message);
    static Alert* Warning(Ctx* cx, Str id, Str message);
    static Alert* Error(Ctx* cx, Str id, Str message);
    Alert* Title(Str s);
    Alert* Icon(IconName n);
    Alert* Content(El* e);
    Alert* Banner();
    Alert* Visible(bool v);
    Alert* OnClose(Func0 fn);
    Alert* WithSize(UiSize s);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
