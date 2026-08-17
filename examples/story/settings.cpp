#include "Story.h"

El* SettingsRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(cx, "Default",
                           "A settings page with groups and typed fields.");
    StorySectionAdd(
        sec, component::Setting::New(cx, StrL("General"))
                 ->Item(StrL("Display name"),
                        component::Input::New(cx, StrL("set-name"), &app->field)
                            ->IntoEl())
                 ->Item(StrL("Automatic updates"),
                        component::Switch::New(cx, StrL("set-sw"))
                            ->Checked(app->switchOn)
                            ->IntoEl())
                 ->IntoEl());
    page->Child(sec);

    El* editor = StorySection(cx, "Editor", nullptr);
    StorySectionAdd(
        editor,
        component::Setting::New(cx, StrL("Editor"))
            ->Item(StrL("Tab size"),
                   component::NumberInput::New(cx, &app->field)->IntoEl())
            ->Item(StrL("Word wrap"),
                   component::Switch::New(cx, StrL("set-wrap"))
                       ->Checked(true)
                       ->IntoEl())
            ->IntoEl());
    page->Child(editor);
    return page;
}

void SettingsClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StorySettings, SettingsRender, SettingsClick);
