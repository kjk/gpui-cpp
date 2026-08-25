#include "Story.h"

// The toolbar's two dropdowns, and what each of their rows does.
enum {
    NotifMenuPlacement = 1,
    NotifMenuMaxItems
};
enum {
    NotifActPlacement = 3500,
    NotifActMaxItems = 3520
};

// ANCHORS, and MAX_ITEMS.
static const component::NotificationAnchor kAnchors[] = {
    component::NotificationAnchor::TopLeft,
    component::NotificationAnchor::TopCenter,
    component::NotificationAnchor::TopRight,
    component::NotificationAnchor::LeftCenter,
    component::NotificationAnchor::RightCenter,
    component::NotificationAnchor::BottomLeft,
    component::NotificationAnchor::BottomCenter,
    component::NotificationAnchor::BottomRight,
};
static const char* const kAnchorNames[] = {
    "TopLeft",     "TopCenter",  "TopRight",     "LeftCenter",
    "RightCenter", "BottomLeft", "BottomCenter", "BottomRight"};
static const int kNAnchors = 8;
static const int kMaxItems[] = {1, 2, 3, 5, 10};
static const int kNMaxItems = 5;

struct NotificationStory {
    // NotificationList is a view in Rust, held by Root and reached through
    // `window.notifications(cx)`. It is the app's here for the same reason:
    // the page pushes into it, but a notification is the window's and outlives
    // leaving this page.
    int openMenu = 0;

    static El* Render(NotificationStory* self, Ctx* cx);
};

static void NotifMenuOpen(NotificationStory* self, Ctx* cx, const ClickEvent*,
                          intptr_t which) {
    self->openMenu = self->openMenu == (int)which ? 0 : (int)which;
    Notify(cx);
}

// Both dropdowns write into the list itself, which is where Rust keeps them
// (Theme::notification.placement / max_items).
static void NotifMenuAct(NotificationStory* self, Ctx* cx, const ClickEvent*,
                         intptr_t act) {
    component::NotificationListState* st = StoryNotifications(cx).Get(cx);
    if (st && act >= NotifActMaxItems) {
        st->maxItems = kMaxItems[act - NotifActMaxItems];
    } else if (st && act >= NotifActPlacement) {
        st->placement = kAnchors[act - NotifActPlacement];
    }
    self->openMenu = 0;
    Notify(cx);
}

// What each button pushes. The id is 0 for a notification that stacks and a
// fixed one for the unique and keyed ones, which is what NotificationId does
// in Rust: the same id replaces rather than stacking a second copy.
struct NotifySpec {
    int id;
    component::NotificationKind kind;
    const char* title;
    const char* message;
    component::NotificationAnchor anchor;
    // autohide(false) is a timeout of zero: it stays until it is dismissed.
    int timeoutMs;
};

static const int kNotifyTimeout = 5000;

static NotifySpec NotifySpecFor(int which) {
    using K = component::NotificationKind;
    using A = component::NotificationAnchor;
    switch (which) {
        case 0:
            return {0,       K::Info,       nullptr, "This is a notification.",
                    A::None, kNotifyTimeout};
        case 1:
            return {0,
                    K::Info,
                    nullptr,
                    "You have been saved file "
                    "successfully.",
                    A::None,
                    kNotifyTimeout};
        case 2:
            return {0,       K::Success,
                    nullptr, "We have received your payment successfully.",
                    A::None, kNotifyTimeout};
        case 3:
            return {0,
                    K::Warning,
                    nullptr,
                    "The network is not stable, please check your connection.",
                    A::None,
                    kNotifyTimeout};
        case 4:
            return {0,
                    K::Error,
                    nullptr,
                    "There have some error occurred. Please try again later.",
                    A::None,
                    kNotifyTimeout};
        case 5:
        case 6:
        case 7:
        case 8: {
            K kind = which == 6   ? K::Success
                     : which == 7 ? K::Warning
                     : which == 8 ? K::Error
                                  : K::Info;
            return {0,
                    kind,
                    "All changes saved",
                    "Your changes have been saved to the cloud and will sync "
                    "across all of your devices.",
                    A::None,
                    kNotifyTimeout};
        }
        case 9:
            return {900,     K::Info,
                    nullptr, "Only one of these is ever on screen at a time.",
                    A::None, kNotifyTimeout};
        case 10:
            return {910,     K::Info,       nullptr, "Notification A",
                    A::None, kNotifyTimeout};
        case 11:
            return {911,     K::Info,       nullptr, "Notification B",
                    A::None, kNotifyTimeout};
        // The system-delivered pair, both under one id: a second push
        // replaces the first in the notification center as it does in the
        // stack.
        case 50:
            return {950,
                    K::Info,
                    "Build finished",
                    "Delivered straight to the notification center.",
                    A::None,
                    kNotifyTimeout};
        case 51:
            return {950,
                    K::Info,
                    "Build finished",
                    "Shown as a toast and in the notification center.",
                    A::None,
                    0};
        case 21:
            return {0,
                    K::Info,
                    "on_click vs on_close",
                    "Click the body to fire on_click; click the X to close. "
                    "Watch the console.",
                    A::None,
                    kNotifyTimeout};
        default:
            return {0,
                    K::Info,
                    nullptr,
                    "You can close this notification by clicking the Close "
                    "button.",
                    A::None,
                    0};
    }
}

static void ClickNote(NotificationStory*, Ctx*, const ClickEvent*) {
    log(StrL("[notification] on_click fired\n"));
}

static void ClickSystemNote(NotificationStory*, Ctx*, const ClickEvent*) {
    log(StrL("[notification] system notification clicked\n"));
}

static void ShowNotify(NotificationStory*, Ctx* cx, const ClickEvent*,
                       intptr_t which) {
    component::NotificationListState* st = StoryNotifications(cx).Get(cx);
    if (!st) {
        return;
    }
    NotifySpec spec = NotifySpecFor((int)which);
    component::NotificationItem item;
    item.id = spec.id;
    item.kind = spec.kind;
    item.title = spec.title ? Str(spec.title) : Str{};
    item.message = Str(spec.message);
    if (which == 21) {
        item.onClick = Listen(cx, &ClickNote);
    }
    // .system() and .in_app_and_system(): where this one goes, whatever the
    // list's own delivery is.
    if (which == 50 || which == 51) {
        item.hasDelivery = true;
        item.delivery = which == 50
                            ? component::NotificationDelivery::System
                            : component::NotificationDelivery::InAppAndSystem;
        item.onClick = Listen(cx, &ClickSystemNote);
    }
    // Placement per notification: the buttons in that section move the whole
    // stack, which is the corner a notification without one of its own goes
    // to as well.
    if (which >= 30 && which < 38) {
        st->placement =
            (component::NotificationAnchor)((int)which - 30 +
                                            (int)component::NotificationAnchor::
                                                TopLeft);
        item.message = StrL("This notification is at the new placement.");
    }
    NotificationPush(st, cx, item, spec.timeoutMs);
    Notify(cx);
}

// Dismiss All: every notification starts on its way out.
static void DismissAll(NotificationStory*, Ctx* cx, const ClickEvent*) {
    component::NotificationListState* st = StoryNotifications(cx).Get(cx);
    if (st) {
        NotificationClear(st, cx);
    }
    Notify(cx);
}

El* NotificationStory::Render(NotificationStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(12)->W(kFill);

    // story_toolbar_group(): the placement and max-items dropdowns.
    component::NotificationListState* st = StoryNotifications(cx).Get(cx);
    int placementIx = 2, maxIx = 4;
    for (int i = 0; i < kNAnchors; i++) {
        if (st && st->placement == kAnchors[i]) {
            placementIx = i;
        }
    }
    for (int i = 0; i < kNMaxItems; i++) {
        if (st && st->maxItems == kMaxItems[i]) {
            maxIx = i;
        }
    }
    Listener openMenu = Listen(cx, &NotifMenuOpen);
    Listener act = Listen(cx, &NotifMenuAct);
    El* toolbarRow = Div(a)->FlexRow()->W(kFill)->JustifyEnd()->ItemsStart();
    El* group = StoryToolbarGroup(cx);
    StoryToolbarOpt anchorRows[kNAnchors];
    for (int i = 0; i < kNAnchors; i++) {
        anchorRows[i].label = kAnchorNames[i];
        anchorRows[i].checked = placementIx == i;
        anchorRows[i].act = NotifActPlacement + i;
    }
    group->Child(StoryToolbarDropdown(
        cx, StrL("placement"),
        StoryFmt(cx, "Placement: %s", kAnchorNames[placementIx]),
        self->openMenu == NotifMenuPlacement,
        ListenerArg(openMenu, NotifMenuPlacement), anchorRows, kNAnchors, act));
    group->Child(StoryToolbarDivider(cx));
    StoryToolbarOpt maxRows[kNMaxItems];
    for (int i = 0; i < kNMaxItems; i++) {
        maxRows[i].label = i == 0   ? "1"
                           : i == 1 ? "2"
                           : i == 2 ? "3"
                           : i == 3 ? "5"
                                    : "10";
        maxRows[i].checked = maxIx == i;
        maxRows[i].act = NotifActMaxItems + i;
    }
    group->Child(StoryToolbarDropdown(
        cx, StrL("max-items"), StoryFmt(cx, "Max items: %d", kMaxItems[maxIx]),
        self->openMenu == NotifMenuMaxItems,
        ListenerArg(openMenu, NotifMenuMaxItems), maxRows, kNMaxItems, act));
    toolbarRow->Child(group);
    page->Child(toolbarRow);

    El* def = StorySection(cx, "Default", "Show a short message.");
    StorySectionAdd(def, component::Button::New(cx, StrL("show-notify-0"))
                             ->OnClick(Listen(cx, &ShowNotify, 0))
                             ->Outline()
                             ->Label(StrL("Show Notification"))
                             ->IntoEl());
    page->Child(def);

    El* types = StorySection(cx, "Types",
                             "Use semantic treatments for common outcomes.");
    StorySectionAdd(types, component::Button::New(cx, StrL("show-notify-info"))
                               ->OnClick(Listen(cx, &ShowNotify, 1))
                               ->Info()
                               ->Label(StrL("Info"))
                               ->IntoEl());
    StorySectionAdd(types,
                    component::Button::New(cx, StrL("show-notify-success"))
                        ->OnClick(Listen(cx, &ShowNotify, 2))
                        ->Success()
                        ->Label(StrL("Success"))
                        ->IntoEl());
    StorySectionAdd(types,
                    component::Button::New(cx, StrL("show-notify-warning"))
                        ->OnClick(Listen(cx, &ShowNotify, 3))
                        ->Warning()
                        ->Label(StrL("Warning"))
                        ->IntoEl());
    StorySectionAdd(types, component::Button::New(cx, StrL("show-notify-error"))
                               ->OnClick(Listen(cx, &ShowNotify, 4))
                               ->Danger()
                               ->Label(StrL("Error"))
                               ->IntoEl());
    page->Child(types);

    El* titled = StorySection(cx, "Title and description",
                              "Pair a concise title with supporting detail.");
    // One button per type, as the story has.
    StorySectionAdd(titled, component::Button::New(cx, StrL("show-typed-info"))
                                ->OnClick(Listen(cx, &ShowNotify, 5))
                                ->Info()
                                ->Label(StrL("Info"))
                                ->IntoEl());
    StorySectionAdd(titled,
                    component::Button::New(cx, StrL("show-typed-success"))
                        ->OnClick(Listen(cx, &ShowNotify, 6))
                        ->Success()
                        ->Label(StrL("Success"))
                        ->IntoEl());
    StorySectionAdd(titled,
                    component::Button::New(cx, StrL("show-typed-warning"))
                        ->OnClick(Listen(cx, &ShowNotify, 7))
                        ->Warning()
                        ->Label(StrL("Warning"))
                        ->IntoEl());
    StorySectionAdd(titled, component::Button::New(cx, StrL("show-typed-error"))
                                ->OnClick(Listen(cx, &ShowNotify, 8))
                                ->Danger()
                                ->Label(StrL("Error"))
                                ->IntoEl());
    page->Child(titled);

    El* unique =
        StorySection(cx, "Unique", "Replace duplicate notifications by type.");
    StorySectionAdd(unique, component::Button::New(cx, StrL("unique-notify"))
                                ->OnClick(Listen(cx, &ShowNotify, 9))
                                ->Outline()
                                ->Label(StrL("Unique Notification"))
                                ->IntoEl());
    page->Child(unique);

    El* keyed = StorySection(cx, "Keyed",
                             "Keep separate unique notifications with keys.");
    El* keyRow = Div(a)->FlexRow()->FlexWrap()->Gap(8)->ItemsCenter();
    keyRow->Child(component::Button::New(cx, StrL("keyed-a"))
                      ->OnClick(Listen(cx, &ShowNotify, 10))
                      ->Outline()
                      ->Label(StrL("A Notification"))
                      ->IntoEl());
    keyRow->Child(component::Button::New(cx, StrL("keyed-b"))
                      ->OnClick(Listen(cx, &ShowNotify, 11))
                      ->Outline()
                      ->Label(StrL("B Notification"))
                      ->IntoEl());
    StorySectionAdd(keyed, keyRow);
    page->Child(keyed);

    // Action: an inline button inside the notification.
    El* actionSec =
        StorySection(cx, "Action", "Add an inline action to the notification.");
    StorySectionAdd(actionSec,
                    component::Button::New(cx, StrL("show-notify-with-title"))
                        ->OnClick(Listen(cx, &ShowNotify, 20))
                        ->Outline()
                        ->Label(StrL("Notification with Title"))
                        ->IntoEl());
    page->Child(actionSec);

    // Lifecycle: the body and the x report separately.
    El* life = StorySection(
        cx, "Lifecycle", "Handle body clicks and close events independently.");
    StorySectionAdd(life,
                    component::Button::New(cx, StrL("show-notify-click-close"))
                        ->OnClick(Listen(cx, &ShowNotify, 21))
                        ->Outline()
                        ->Label(StrL("Click vs Close"))
                        ->IntoEl());
    page->Child(life);

    // Placement per notification: one button per anchor.
    El* place = StorySection(cx, "Placement per notification",
                             "Override the global placement for a single "
                             "notification.");
    for (int i = 0; i < kNAnchors; i++) {
        StorySectionAdd(place,
                        component::Button::New(
                            cx, StoryFmt(cx, "show-notify-%s", kAnchorNames[i]))
                            ->OnClick(Listen(cx, &ShowNotify, 30 + i))
                            ->Outline()
                            ->Label(Str(kAnchorNames[i]))
                            ->IntoEl());
    }
    page->Child(place);

    // The system half: the same notification handed to the OS notification
    // center rather than — or as well as — the in-app stack.
    El* system = StorySection(
        cx, "System notification",
        "Deliver to the OS notification center; click the system "
        "notification to bring the window back. Windows shows these in the "
        "Action Center; macOS, Linux and the browser have no backend here "
        "and fall back to the in-app toast.");
    StorySectionAdd(system,
                    component::Button::New(cx, StrL("show-notify-system"))
                        ->OnClick(Listen(cx, &ShowNotify, 50))
                        ->Outline()
                        ->Label(StrL("System only"))
                        ->IntoEl());
    StorySectionAdd(system, component::Button::New(
                                cx, StrL("show-notify-in-app-and-system"))
                                ->OnClick(Listen(cx, &ShowNotify, 51))
                                ->Outline()
                                ->Label(StrL("In-app and system"))
                                ->IntoEl());
    page->Child(system);

    // Custom content: markdown the application owns.
    El* customSec = StorySection(
        cx, "Custom content", "Render application-owned notification content.");
    StorySectionAdd(customSec,
                    component::Button::New(cx, StrL("show-notify-custom"))
                        ->OnClick(Listen(cx, &ShowNotify, 40))
                        ->Outline()
                        ->Label(StrL("Show Custom Notification"))
                        ->IntoEl());
    page->Child(customSec);

    // Manual close: autohide(false), so only Dismiss All takes it away.
    El* manual = StorySection(cx, "Manual close",
                              "Keep a notification visible until it is "
                              "dismissed.");
    StorySectionAdd(manual,
                    component::Button::New(cx, StrL("manual-open-notify"))
                        ->OnClick(Listen(cx, &ShowNotify, 41))
                        ->Outline()
                        ->Label(StrL("Show"))
                        ->IntoEl());
    StorySectionAdd(manual,
                    component::Button::New(cx, StrL("manual-close-notify"))
                        ->OnClick(Listen(cx, &DismissAll))
                        ->Outline()
                        ->Label(StrL("Dismiss All"))
                        ->IntoEl());
    page->Child(manual);
    // The stack itself, over the window in whichever corner its placement
    // names — what Root renders in Rust.
    return page;
}

STORY_PAGE(StoryNotification, NotificationStory);
