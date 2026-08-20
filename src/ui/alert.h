/* Themed alert — crates/ui/src/alert.rs */

#include "ui/sizing.h"

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
    // crates/ui's `message` is a `Text`, which is either a plain string or the
    // `markdown(..)` the story hands two of these. Both render through the one
    // rich-text view when this is set.
    bool markdown = false;
    // Not in crates/ui: Rust's Notification is an element of its own and this
    // one borrows the alert's frame, so it needs a slot for the body it
    // builds. Nothing in a story uses it.
    El* content = nullptr;
    UiSize size = UiSize::Medium;
    bool banner = false;
    bool visible = true;
    Listener onClose;

    static Alert* New(Ctx* cx, Str id, Str message);
    static Alert* Info(Ctx* cx, Str id, Str message);
    static Alert* Success(Ctx* cx, Str id, Str message);
    static Alert* Warning(Ctx* cx, Str id, Str message);
    static Alert* Error(Ctx* cx, Str id, Str message);
    Alert* Title(Str s);
    Alert* Icon(IconName n);
    Alert* Markdown(bool v = true);
    Alert* Content(El* e);
    Alert* Banner();
    Alert* Visible(bool v);
    Alert* OnClose(Listener fn);
    Alert* WithSize(UiSize s);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
