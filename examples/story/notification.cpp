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
    StorySectionAdd(titled, component::Button::New(cx, StrL("show-typed-info"))
                                ->OnClick(Listen(cx, &ShowNotify, 5))
                                ->Info()
                                ->Label(StrL("Info"))
                                ->IntoEl());
    if (self->notifyOn && self->selA == 5) {
        StorySectionAdd(
            titled,
            component::Notification::New(
                cx, StrL("All changes saved"),
                StrL("Your changes have been saved to the cloud and will sync "
                     "across all of your devices."))
                ->Kind(component::NotificationKind::Info)
                ->OnClose(Listen(cx, &HideNote))
                ->IntoEl());
    }
    page->Child(titled);
    return page;
}

STORY_PAGE(StoryNotification, NotificationStory);
