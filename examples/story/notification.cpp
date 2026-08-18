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

static void HideNote(NotificationStory* self, Ctx* cx, const ClickEvent*) {
    self->notifyOn = false;
}

El* NotificationStory::Render(NotificationStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

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
    return page;
}

STORY_PAGE(StoryNotification, NotificationStory);
