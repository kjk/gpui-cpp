/* Themed notification — crates/ui/src/notification.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

enum class NotificationKind : u8 {
    Info,
    Success,
    Warning,
    Error
};

struct Notification {
    Arena* a = nullptr;
    NotificationKind kind = NotificationKind::Info;
    Str title = {};
    Str message = {};
    Func0 onClose;

    static Notification* New(Arena* a, Str title, Str message);
    Notification* Kind(NotificationKind k);
    Notification* OnClose(Func0 fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
