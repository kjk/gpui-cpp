#include "Story.h"

struct SettingsStory {
    LineInput field = {};
    bool switchOn = true;
    bool seeded = false;

    static El* Render(SettingsStory* self, Ctx* cx);
    static void Click(SettingsStory* self, Ctx* cx, int id);
};

El* SettingsStory::Render(SettingsStory* self, Ctx* cx) {
    Arena* a = cx->a;
    if (!self->seeded) {
        self->seeded = true;
        strncpy_s(self->field.placeholder, "Type something…", _TRUNCATE);
        strncpy_s(self->field.buf, "Hello GPUI", _TRUNCATE);
        self->field.len = (int)strlen(self->field.buf);
    }
    if (self->field.focused) {
        cx->win->input = &self->field;
    }
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(cx, "Default",
                           "A settings page with groups and typed fields.");
    StorySectionAdd(sec, component::Setting::New(cx, StrL("General"))
                             ->Item(StrL("Display name"),
                                    component::Input::New(cx, StrL("set-name"),
                                                          &self->field)
                                        ->IntoEl())
                             ->Item(StrL("Automatic updates"),
                                    component::Switch::New(cx, StrL("set-sw"))
                                        ->Checked(self->switchOn)
                                        ->IntoEl())
                             ->IntoEl());
    page->Child(sec);

    El* editor = StorySection(cx, "Editor", nullptr);
    StorySectionAdd(
        editor,
        component::Setting::New(cx, StrL("Editor"))
            ->Item(StrL("Tab size"),
                   component::NumberInput::New(cx, &self->field)->IntoEl())
            ->Item(StrL("Word wrap"),
                   component::Switch::New(cx, StrL("set-wrap"))
                       ->Checked(true)
                       ->IntoEl())
            ->IntoEl());
    page->Child(editor);
    return page;
}

void SettingsStory::Click(SettingsStory* self, Ctx* cx, int id) {
    (void)cx;
    (void)id;
}

STORY_PAGE(StorySettings, SettingsStory);
