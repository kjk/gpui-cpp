#include "Story.h"

static void HideNote(StoryApp* app) {
    app->notifyOn = false;
}

El* NotificationRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* def = StorySection(a, "Default", "Show a short message.");
    StorySectionAdd(def, component::Button::New(a, StrL("show-notify-0"))
                             ->Outline()
                             ->Label(StrL("Show Notification"))
                             ->IntoEl());
    if (app->notifyOn && app->selA == 0) {
        StorySectionAdd(def, component::Notification::New(
                                 a, {}, StrL("This is a notification."))
                                 ->OnClose(MkFunc0(&HideNote, app))
                                 ->IntoEl());
    }
    page->Child(def);

    El* types = StorySection(a, "Types",
                             "Use semantic treatments for common outcomes.");
    El* typeRow = Div(a)->FlexRow()->Gap(8)->ItemsCenter()->Wrap();
    typeRow->Child(component::Button::New(a, StrL("show-notify-info"))
                       ->Info()
                       ->Label(StrL("Info"))
                       ->IntoEl());
    typeRow->Child(component::Button::New(a, StrL("show-notify-success"))
                       ->Success()
                       ->Label(StrL("Success"))
                       ->IntoEl());
    typeRow->Child(component::Button::New(a, StrL("show-notify-warning"))
                       ->Warning()
                       ->Label(StrL("Warning"))
                       ->IntoEl());
    typeRow->Child(component::Button::New(a, StrL("show-notify-error"))
                       ->Danger()
                       ->Label(StrL("Error"))
                       ->IntoEl());
    StorySectionAdd(types, typeRow);
    if (app->notifyOn && app->selA >= 1 && app->selA <= 4) {
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
            types, component::Notification::New(a, {}, Str(kMsgs[app->selA]))
                       ->Kind(kKinds[app->selA])
                       ->OnClose(MkFunc0(&HideNote, app))
                       ->IntoEl());
    }
    page->Child(types);

    El* titled = StorySection(a, "Title and description",
                              "Pair a concise title with supporting detail.");
    StorySectionAdd(titled, component::Button::New(a, StrL("show-typed-info"))
                                ->Info()
                                ->Label(StrL("Info"))
                                ->IntoEl());
    if (app->notifyOn && app->selA == 5) {
        StorySectionAdd(
            titled,
            component::Notification::New(
                a, StrL("All changes saved"),
                StrL("Your changes have been saved to the cloud and will sync "
                     "across all of your devices."))
                ->Kind(component::NotificationKind::Info)
                ->OnClose(MkFunc0(&HideNote, app))
                ->IntoEl());
    }
    page->Child(titled);
    return page;
}

void NotificationClick(StoryApp* app, int id) {
    if (id == HashClickId(StrL("show-notify-0"))) {
        app->notifyOn = true;
        app->selA = 0;
    } else if (id == HashClickId(StrL("show-notify-info"))) {
        app->notifyOn = true;
        app->selA = 1;
    } else if (id == HashClickId(StrL("show-notify-success"))) {
        app->notifyOn = true;
        app->selA = 2;
    } else if (id == HashClickId(StrL("show-notify-warning"))) {
        app->notifyOn = true;
        app->selA = 3;
    } else if (id == HashClickId(StrL("show-notify-error"))) {
        app->notifyOn = true;
        app->selA = 4;
    } else if (id == HashClickId(StrL("show-typed-info"))) {
        app->notifyOn = true;
        app->selA = 5;
    }
}

STORY_PAGE(StoryNotification, NotificationRender, NotificationClick);
