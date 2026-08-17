#include "Story.h"

El* SettingsRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default",
                           "A settings page with groups and typed fields.");
    StorySectionAdd(
        sec, component::Setting::New(a, StrL("General"))
                 ->Item(StrL("Display name"),
                        component::Input::New(a, StrL("set-name"), &app->field)
                            ->IntoEl())
                 ->Item(StrL("Automatic updates"),
                        component::Switch::New(a, StrL("set-sw"))
                            ->Checked(app->switchOn)
                            ->IntoEl())
                 ->IntoEl());
    page->Child(sec);
    return page;
}

void SettingsClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StorySettings, SettingsRender, SettingsClick);
