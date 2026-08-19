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

// TITLE_BAR_HEIGHT + 16: the margin a floating stack keeps from the window
// edges, so it clears the title bar.
const float kNotificationMargin = 16;
// DEFAULT_NOTIFICATION_WIDTH.
const float kNotificationWidth = 382;
// NotificationSettings::max_items.
const int kNotificationMaxItems = 10;
// How often the list is advanced. Rust spawns a task that ticks at this rate.
const int kNotificationTickMs = 50;

// One notification in the list. Rust's is an entity with its own render; the
// content here is what the card shows, and a caller that wants more puts an
// element in `content`.
struct NotificationItem {
    int id = 0;
    NotificationKind kind = NotificationKind::Info;
    Str title = {};
    Str message = {};
    El* content = nullptr;
    Listener onClick = {};
};

// NotificationList: the notifications on screen, when each of them goes away,
// and which corner they stack in. The lifecycle is the toast stack's, so a
// notification is a toast that happens to look like one.
struct NotificationListState {
    ToastStackState stack = {};
    NotificationItem items[kToastStackCap] = {};
    int n = 0;
    int nextId = 1;
    NotificationAnchor placement = NotificationAnchor::TopRight;
    float width = kNotificationWidth;
    int maxItems = kNotificationMaxItems;
    // The height a card is assumed to be while the stack is laid out. Rust
    // measures each one; every card here is the same shape, so one number is
    // as good.
    float itemH = 76;
    // How far in the transition each card is, so the tick can be turned into
    // an offset. Written by NotificationAdvance.
    double lastTickAt = 0;

    static void OnCloseClick(NotificationListState* self, Ctx* cx,
                             const ClickEvent* ev, intptr_t id);
    static void OnItemClick(NotificationListState* self, Ctx* cx,
                            const ClickEvent* ev, intptr_t id);
    static void OnHover(NotificationListState* self, Ctx* cx,
                        const HoverEvent* ev);
    static void OnTick(NotificationListState* self, Ctx* cx,
                       const TickEvent* ev);
};

// push(): add a notification and answer its id. `timeoutMs` of 0 is Rust's
// `autohide(false)` — it stays until it is dismissed. The oldest goes when
// the list is over `maxItems`, and a push with an id already in the list
// replaces that one rather than stacking a second copy, which is what Rust's
// NotificationId does.
int NotificationPush(NotificationListState* s, NotificationItem item,
                     int timeoutMs);
// dismiss(): start the one with this id on its way out.
void NotificationDismiss(NotificationListState* s, int id);
// clear().
void NotificationClear(NotificationListState* s);
// advance(): move every notification on by `deltaMs`, dropping the ones that
// finished leaving. Answers whether anything changed.
bool NotificationAdvance(NotificationListState* s, int deltaMs);
// Where the item with this id is, or -1.
int NotificationIndexOf(const NotificationListState* s, int id);

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

// The floating stack, drawn over the window in the corner its placement
// names — what Root does with the notification list in Rust.
struct NotificationList {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Entity<NotificationListState> state = {};

    static NotificationList* New(Ctx* cx, Entity<NotificationListState> state);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
