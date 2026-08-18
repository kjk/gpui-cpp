/* Themed notification — crates/ui/src/notification.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

enum class NotificationKind : uint8_t {
    Info,
    Success,
    Warning,
    Error
};

struct Notification {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    NotificationKind kind = NotificationKind::Info;
    Str title = {};
    Str message = {};
    Listener onClose;

    static Notification* New(Ctx* cx, Str title, Str message);
    Notification* Kind(NotificationKind k);
    Notification* OnClose(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
