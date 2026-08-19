#include "Story.h"

struct NotificationStory {
    // NotificationList is a view in Rust, held by Root; here it is a state
    // the page owns and renders over the window.
    Entity<component::NotificationListState> list = {};
    bool seeded = false;
    int timer = 0;

    static El* Render(NotificationStory* self, Ctx* cx);
};

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

static void ShowNotify(NotificationStory* self, Ctx* cx, const ClickEvent*,
                       intptr_t which) {
    component::NotificationListState* st = self->list.Get(cx);
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
    NotificationPush(st, item, spec.timeoutMs);
    Notify(cx);
}

// Dismiss All: every notification starts on its way out.
static void DismissAll(NotificationStory* self, Ctx* cx, const ClickEvent*) {
    component::NotificationListState* st = self->list.Get(cx);
    if (st) {
        NotificationClear(st);
    }
    Notify(cx);
}

El* NotificationStory::Render(NotificationStory* self, Ctx* cx) {
    Arena* a = cx->a;
    if (!self->seeded) {
        self->seeded = true;
        self->list = EntityNewState<component::NotificationListState>(cx->app);
        // Rust spawns a task that advances the list every 50 ms; a window
        // timer is the same clock.
        self->timer = WindowSetInterval(
            cx->win, component::kNotificationTickMs,
            ListenTo(self->list, &component::NotificationListState::OnTick));
    }
    El* page = Div(a)->FlexCol()->Gap(12)->W(kFill);

    El* def = StorySection(cx, "Default", "Show a short message.");
    StorySectionAdd(def, component::Button::New(cx, StrL("show-notify-0"))
                             ->OnClick(Listen(cx, &ShowNotify, 0))
                             ->Outline()
                             ->Label(StrL("Show Notification"))
                             ->IntoEl());
    page->Child(def);

    El* types = StorySection(cx, "Types",
                             "Use semantic treatments for common outcomes.");
    El* typeRow = Div(a)->FlexRow()->Gap(8)->ItemsCenter()->Wrap();
    typeRow->Child(component::Button::New(cx, StrL("show-notify-info"))
                       ->OnClick(Listen(cx, &ShowNotify, 1))
                       ->Info()
                       ->Label(StrL("Info"))
                       ->IntoEl());
    typeRow->Child(component::Button::New(cx, StrL("show-notify-success"))
                       ->OnClick(Listen(cx, &ShowNotify, 2))
                       ->Success()
                       ->Label(StrL("Success"))
                       ->IntoEl());
    typeRow->Child(component::Button::New(cx, StrL("show-notify-warning"))
                       ->OnClick(Listen(cx, &ShowNotify, 3))
                       ->Warning()
                       ->Label(StrL("Warning"))
                       ->IntoEl());
    typeRow->Child(component::Button::New(cx, StrL("show-notify-error"))
                       ->OnClick(Listen(cx, &ShowNotify, 4))
                       ->Danger()
                       ->Label(StrL("Error"))
                       ->IntoEl());
    StorySectionAdd(types, typeRow);
    page->Child(types);

    El* titled = StorySection(cx, "Title and description",
                              "Pair a concise title with supporting detail.");
    // One button per type, as the story has.
    El* titledRow = Div(a)->FlexRow()->FlexWrap()->Gap(8)->ItemsCenter();
    titledRow->Child(component::Button::New(cx, StrL("show-typed-info"))
                         ->OnClick(Listen(cx, &ShowNotify, 5))
                         ->Info()
                         ->Label(StrL("Info"))
                         ->IntoEl());
    titledRow->Child(component::Button::New(cx, StrL("show-typed-success"))
                         ->OnClick(Listen(cx, &ShowNotify, 6))
                         ->Success()
                         ->Label(StrL("Success"))
                         ->IntoEl());
    titledRow->Child(component::Button::New(cx, StrL("show-typed-warning"))
                         ->OnClick(Listen(cx, &ShowNotify, 7))
                         ->Warning()
                         ->Label(StrL("Warning"))
                         ->IntoEl());
    titledRow->Child(component::Button::New(cx, StrL("show-typed-error"))
                         ->OnClick(Listen(cx, &ShowNotify, 8))
                         ->Danger()
                         ->Label(StrL("Error"))
                         ->IntoEl());
    StorySectionAdd(titled, titledRow);
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
    struct AnchorSpec {
        const char* label;
        component::NotificationAnchor anchor;
    };
    static const AnchorSpec kAnchors[] = {
        {"TopLeft", component::NotificationAnchor::TopLeft},
        {"TopCenter", component::NotificationAnchor::TopCenter},
        {"TopRight", component::NotificationAnchor::TopRight},
        {"LeftCenter", component::NotificationAnchor::LeftCenter},
        {"RightCenter", component::NotificationAnchor::RightCenter},
        {"BottomLeft", component::NotificationAnchor::BottomLeft},
        {"BottomCenter", component::NotificationAnchor::BottomCenter},
        {"BottomRight", component::NotificationAnchor::BottomRight},
    };
    El* anchorRow = Div(a)->FlexRow()->FlexWrap()->Gap(8)->ItemsCenter();
    for (size_t i = 0; i < sizeof(kAnchors) / sizeof(kAnchors[0]); i++) {
        anchorRow
            ->Child(component::Button::New(
                        cx, StoryFmt(cx, "show-notify-%s", kAnchors[i].label))
                        ->OnClick(Listen(cx, &ShowNotify, 30 + (int)i))
                        ->Outline()
                        ->Label(Str(kAnchors[i].label))
                        ->IntoEl());
    }
    StorySectionAdd(place, anchorRow);
    page->Child(place);

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
    El* manualRow = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    manualRow->Child(component::Button::New(cx, StrL("manual-open-notify"))
                         ->OnClick(Listen(cx, &ShowNotify, 41))
                         ->Outline()
                         ->Label(StrL("Show"))
                         ->IntoEl());
    manualRow->Child(component::Button::New(cx, StrL("manual-close-notify"))
                         ->OnClick(Listen(cx, &DismissAll))
                         ->Outline()
                         ->Label(StrL("Dismiss All"))
                         ->IntoEl());
    StorySectionAdd(manual, manualRow);
    page->Child(manual);
    // The stack itself, over the window in whichever corner its placement
    // names — what Root renders in Rust.
    page->Child(component::NotificationList::New(cx, self->list)->IntoEl());
    return page;
}

STORY_PAGE(StoryNotification, NotificationStory);
