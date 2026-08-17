#include "Story.h"

enum { ClickStoryNotify = 2730 };

static void HideNote(StoryApp* app) {
    app->notifyOn = false;
}

El* NotificationRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default", "A brief message that appears temporarily.");
    StorySectionAdd(sec, component::Button::New(a, StrL("show-note"))->Label(StrL("Show notification"))->Outline()->IntoEl());
    if (app->notifyOn) {
        StorySectionAdd(sec, component::Notification::New(a, StrL("Changes saved"), StrL("Your preferences are now up to date."))
                                 ->Kind(component::NotificationKind::Success)
                                 ->OnClose(MkFunc0(&HideNote, app))
                                 ->IntoEl());
    }
    page->Child(sec);
    return page;
}

void NotificationClick(StoryApp* app, int id) {
    if (id == ClickStoryNotify || id == HashClickId(StrL("show-note"))) {
        app->notifyOn = true;
    }
}

STORY_PAGE(StoryNotification, NotificationRender, NotificationClick);
