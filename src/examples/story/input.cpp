#include "Story.h"

enum { ClickStoryField = 2600 };

El* InputRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default", "Displays a form input field.");
    StorySectionAdd(sec, component::Input::New(a, StrL("name"), &app->field)->Label(StrL("Display name"))->IntoEl());
    page->Child(sec);
    return page;
}

void InputClick(StoryApp* app, int id) {
    if (id == ClickStoryField || id == HashClickId(StrL("name"))) {
        app->field.focused = true;
    }
}

STORY_PAGE(StoryInput, InputRender, InputClick);
