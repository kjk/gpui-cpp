#include "Story.h"

El* FormRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default", "Building forms with labeled fields.");
    StorySectionAdd(sec, component::Form::New(a)
                             ->Field(StrL("Name"), component::Input::New(a, StrL("form-name"), &app->field)->IntoEl())
                             ->Field(StrL("Notify"), component::Switch::New(a, StrL("form-sw"))->Checked(app->switchOn)->IntoEl())
                             ->IntoEl());
    page->Child(sec);
    return page;
}

void FormClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryForm, FormRender, FormClick);
