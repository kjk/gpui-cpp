/* Themed notification — crates/ui/src/notification.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

enum class NotificationKind : uint8_t {
    // Notification::type_ is an Option, and None is the default: no icon at
    // all, and the message where an icon would have pushed it across. A
    // plain `push_notification("...")` is one of these.
    None,
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

// NotificationDelivery: where a notification is presented — as an in-app
// toast, in the operating system's notification center, or both. Only the
// title and the message reach the system half; an action, a content element
// and a placement are the toast's alone.
enum class NotificationDelivery : uint8_t {
    InApp,
    System,
    InAppAndSystem
};

bool NotificationDeliveryIncludesInApp(NotificationDelivery d);
bool NotificationDeliveryIncludesSystem(NotificationDelivery d);

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
    NotificationKind kind = NotificationKind::None;
    Str title = {};
    Str message = {};
    El* content = nullptr;
    Listener onClick = {};
    // Notification::delivery is an Option: unset takes the list's own
    // NotificationSettings::delivery, which is what `hasDelivery` says.
    bool hasDelivery = false;
    NotificationDelivery delivery = NotificationDelivery::InApp;
};

// NotificationList: the notifications on screen, when each of them goes away,
// and which corner they stack in. The lifecycle is the toast stack's, so a
// notification is a toast that happens to look like one.
struct NotificationListState {
    ToastStackState stack;
    Vec<NotificationItem> items;
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
    // NotificationSettings::delivery: where a notification with none of its
    // own goes.
    NotificationDelivery delivery = NotificationDelivery::InApp;
    // cx.entity().downgrade(): what a system notification's response is
    // dispatched back to, since the response arrives from the platform with
    // no view in hand. Stamped by NotificationList::IntoEl and by whoever
    // makes the entity.
    EntityId self = {};

    static void OnCloseClick(NotificationListState* self, Ctx* cx,
                             const ClickEvent* ev, intptr_t id);
    static void OnItemClick(NotificationListState* self, Ctx* cx,
                            const ClickEvent* ev, intptr_t id);
    static void OnHover(NotificationListState* self, Ctx* cx,
                        const HoverEvent* ev);
    static void OnTick(NotificationListState* self, Ctx* cx,
                       const TickEvent* ev);
    // The user clicked the system notification for `id`: the in-app
    // counterpart closes and on_click fires, on the main thread and after the
    // platform's own event is over.
    static void OnSystemResponse(NotificationListState* self, Ctx* cx,
                                 const ClickEvent* ev, intptr_t id);
};

// push(): add a notification and answer its id. `timeoutMs` of 0 is Rust's
// `autohide(false)` — it stays until it is dismissed. The oldest goes when
// the list is over `maxItems`, and a push with an id already in the list
// replaces that one rather than stacking a second copy, which is what Rust's
// NotificationId does.
//
// `cx` is what the system half needs — the window a response is dispatched
// back to — and a null one is the in-app half on its own, which is what a
// test pushes.
int NotificationPush(NotificationListState* s, Ctx* cx, NotificationItem item,
                     int timeoutMs);
// dismiss(): start the one with this id on its way out, and retract its
// system counterpart. Rust does the second unconditionally: a system-only
// notification has no toast to dismiss and must still be taken back.
void NotificationDismiss(NotificationListState* s, Ctx* cx, int id);
// clear().
void NotificationClear(NotificationListState* s, Ctx* cx);
// advance(): move every notification on by `deltaMs`, dropping the ones that
// finished leaving. Answers whether anything changed.
bool NotificationAdvance(NotificationListState* s, int deltaMs);
// Where the item with this id is, or -1.
int NotificationIndexOf(const NotificationListState* s, int id);

// ─── the system half ─────────────────────────────────────────────────────
//
// SystemNotificationRegistry: what a posted system notification needs when
// its response comes back — which window it was posted from, which list to
// close the toast in, and what to fire. It is app-global because the
// response is: the platform hands back a tag and nothing else.

// MAX_SYSTEM_NOTIFICATION_ENTRIES. Past this the stalest are pruned, so a
// long-running application does not accumulate one entry per notification it
// ever posted.
const int kNotificationSystemMax = 100;

struct NotificationSystemEntry {
    int id = 0;
    EntityId list = {};
    Window* win = nullptr;
    Listener onClick = {};
};

// NotificationId::system_tag: SYSTEM_TAG_PREFIX and the notification's id,
// written into `buf`. Namespaced so a response to a notification the
// application posted itself is not ours to answer.
Str NotificationSystemTag(char* buf, int cap, int id);
// The id inside one of those tags. False for anybody else's.
bool NotificationTagId(Str tag, int* outId);
// notification::init: install the app-global response handler. Idempotent,
// and called by the first system push.
void NotificationInitSystem(App* app);
// The registry, which the push fills in and a response empties.
void NotificationSystemInsert(const NotificationSystemEntry& e);
// Retract the system notification for `id`, but only if it was posted from
// `win`: another window that pushed the same id owns the tag now, and this
// window's dismiss must not take back its notification.
void NotificationSystemDismiss(int id, Window* win);
// Retract every system notification posted from `win`.
void NotificationSystemDismissAll(Window* win);
const NotificationSystemEntry* NotificationSystemFind(int id, Window* win);
int NotificationSystemCount();
int NotificationSystemCount(const App* app);
// What the platform calls with the tag of the notification the user clicked.
void NotificationSystemResponse(Str tag);

struct Notification {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    NotificationKind kind = NotificationKind::None;
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
