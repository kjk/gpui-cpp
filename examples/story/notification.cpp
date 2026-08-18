#include "Story.h"

struct NotificationStory {
    bool notifyOn = false;
    int selA = -1;

    static El* Render(NotificationStory* self, Ctx* cx);
};

static void ShowNotify(NotificationStory* self, Ctx* cx, const ClickEvent*,
                       intptr_t kind) {
    self->notifyOn = true;
    self->selA = (int)kind;
    Notify(cx);
}

static void HideNote(NotificationStory* self, Ctx*, const ClickEvent*) {
    self->notifyOn = false;
}
// The Action notification's Retry, and the body click the Lifecycle section
// tells apart from the close. Both log, as the Rust story prints.
static void RetryNote(NotificationStory* self, Ctx* cx, const ClickEvent*) {
    log(StrL("You have clicked the try again action.\n"));
    self->notifyOn = false;
    Notify(cx);
}
static void ClickNote(NotificationStory*, Ctx* cx, const ClickEvent*) {
    log(StrL("[notification] on_click fired\n"));
    Notify(cx);
}
static void CloseNote(NotificationStory* self, Ctx* cx, const ClickEvent*) {
    log(StrL("[notification] on_close fired\n"));
    self->notifyOn = false;
    Notify(cx);
}
// Manual close: the notification stays until Dismiss All takes it away.
static void DismissAll(NotificationStory* self, Ctx* cx, const ClickEvent*) {
    self->notifyOn = false;
    Notify(cx);
}

El* NotificationStory::Render(NotificationStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(12)->W(kFill);

    El* def = StorySection(cx, "Default", "Show a short message.");
    StorySectionAdd(def, component::Button::New(cx, StrL("show-notify-0"))
                             ->OnClick(Listen(cx, &ShowNotify, 0))
                             ->Outline()
                             ->Label(StrL("Show Notification"))
                             ->IntoEl());
    if (self->notifyOn && self->selA == 0) {
        StorySectionAdd(def, component::Notification::New(
                                 cx, {}, StrL("This is a notification."))
                                 ->OnClose(Listen(cx, &HideNote))
                                 ->IntoEl());
    }
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
    if (self->notifyOn && self->selA >= 1 && self->selA <= 4) {
        static const component::NotificationKind kKinds[] = {
            component::NotificationKind::Info,
            component::NotificationKind::Info,
            component::NotificationKind::Success,
            component::NotificationKind::Warning,
            component::NotificationKind::Error};
        static const char* kMsgs[] = {
            "", "You have been saved file successfully.",
            "We have received your payment successfully.",
            "The network is not stable, please check your connection.",
            "There have some error occurred. Please try again later."};
        StorySectionAdd(
            types, component::Notification::New(cx, {}, Str(kMsgs[self->selA]))
                       ->Kind(kKinds[self->selA])
                       ->OnClose(Listen(cx, &HideNote))
                       ->IntoEl());
    }
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
    if (self->notifyOn && self->selA >= 5 && self->selA <= 8) {
        StorySectionAdd(
            titled,
            component::Notification::New(
                cx, StrL("All changes saved"),
                StrL("Your changes have been saved to the cloud and will sync "
                     "across all of your devices."))
                ->Kind(self->selA == 6   ? component::NotificationKind::Success
                       : self->selA == 7 ? component::NotificationKind::Warning
                       : self->selA == 8 ? component::NotificationKind::Error
                                         : component::NotificationKind::Info)
                ->OnClose(Listen(cx, &HideNote))
                ->IntoEl());
    }
    page->Child(titled);

    El* unique =
        StorySection(cx, "Unique", "Replace duplicate notifications by type.");
    StorySectionAdd(unique, component::Button::New(cx, StrL("unique-notify"))
                                ->OnClick(Listen(cx, &ShowNotify, 9))
                                ->Outline()
                                ->Label(StrL("Unique Notification"))
                                ->IntoEl());
    if (self->notifyOn && self->selA == 9) {
        StorySectionAdd(
            unique, component::Notification::New(
                        cx, Str{},
                        StrL("Only one of these is ever on screen at a time."))
                        ->OnClose(Listen(cx, &HideNote))
                        ->IntoEl());
    }
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
    if (self->notifyOn && (self->selA == 10 || self->selA == 11)) {
        StorySectionAdd(keyed, component::Notification::New(
                                   cx, Str{},
                                   self->selA == 10 ? StrL("Notification A")
                                                    : StrL("Notification B"))
                                   ->OnClose(Listen(cx, &HideNote))
                                   ->IntoEl());
    }
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
    if (self->notifyOn && self->selA == 20) {
        StorySectionAdd(
            actionSec,
            component::Notification::New(
                cx, StrL("Uh oh! Something went wrong."),
                StrL("There was a problem with your request."))
                ->Action(component::Button::New(cx, StrL("try-again"))
                             ->Primary()
                             ->Label(StrL("Retry"))
                             ->OnClick(Listen(cx, &RetryNote))
                             ->IntoEl())
                ->OnClose(Listen(cx, &HideNote))
                ->IntoEl());
    }
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
    if (self->notifyOn && self->selA == 21) {
        StorySectionAdd(
            life, component::Notification::New(
                      cx, StrL("on_click vs on_close"),
                      StrL("Click the body to fire on_click; click the X to "
                           "close. Watch the console."))
                      ->OnClick(Listen(cx, &ClickNote))
                      ->OnClose(Listen(cx, &CloseNote))
                      ->IntoEl());
    }
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
    if (self->notifyOn && self->selA >= 30 && self->selA < 38) {
        const AnchorSpec& at = kAnchors[self->selA - 30];
        StorySectionAdd(
            place, component::Notification::New(
                       cx, Str{},
                       StoryFmt(cx, "This notification is at %s.", at.label))
                       ->Placement(at.anchor)
                       ->OnClose(Listen(cx, &HideNote))
                       ->IntoEl());
    }
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
    if (self->notifyOn && self->selA == 40) {
        StorySectionAdd(
            customSec,
            component::Notification::New(cx, Str{}, Str{})
                ->Content(component::TextView::New(
                              cx, StrL("This is a custom notification.\n"
                                       "- List item 1\n"
                                       "- List item 2\n"
                                       "- [Click here]"
                                       "(https://github.com/longbridge/"
                                       "gpui-component)\n"))
                              ->IntoEl())
                ->OnClose(Listen(cx, &HideNote))
                ->IntoEl());
    }
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
    if (self->notifyOn && self->selA == 41) {
        StorySectionAdd(manual,
                        component::Notification::New(
                            cx, Str{},
                            StrL("You can close this notification by clicking "
                                 "the Close button."))
                            ->OnClose(Listen(cx, &HideNote))
                            ->IntoEl());
    }
    page->Child(manual);
    return page;
}

STORY_PAGE(StoryNotification, NotificationStory);
