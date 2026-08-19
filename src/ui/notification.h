/* Themed notification — crates/ui/src/notification.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

enum class NotificationKind : uint8_t {
    Info,
    Success,
    Warning,
    Error
};

// Notification::placement. None keeps the card where the caller put it;
// anything else floats it in that corner of the window, the way Root renders
// the notification list.
enum class NotificationAnchor : uint8_t {
    None,
    TopLeft,
    TopCenter,
    TopRight,
    LeftCenter,
    RightCenter,
    BottomLeft,
    BottomCenter,
    BottomRight
};

struct Notification {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    NotificationKind kind = NotificationKind::Info;
    Str title = {};
    Str message = {};
    // action(): an inline button under the message.
    El* action = nullptr;
    // content(): application-owned content in place of the message.
    El* content = nullptr;
    NotificationAnchor anchor = NotificationAnchor::None;
    // DEFAULT_NOTIFICATION_WIDTH. Only a floating notification sizes itself;
    // an inline one fills the section it sits in.
    float width = 382;
    Listener onClose;
    // on_click fires for the body, on_close only for the x.
    Listener onClick;

    static Notification* New(Ctx* cx, Str title, Str message);
    Notification* Kind(NotificationKind k);
    Notification* Action(El* e);
    Notification* Content(El* e);
    Notification* Placement(NotificationAnchor p);
    Notification* OnClose(Listener fn);
    Notification* OnClick(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
