#include "Story.h"

static void SetChecked(StoryApp* app, bool v) {
    app->checkboxOn = v;
}

El* CheckboxRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(a, app));
    El* sec =
        StorySection(a, "Default",
                     "A control that toggles between checked and not checked.");
    El* col = Div(a)->FlexCol()->Gap(8);
    col->Child(component::Checkbox::New(a, StrL("terms"))
                   ->Label(StrL("Accept terms and conditions"))
                   ->Checked(app->checkboxOn)
                   ->WithSize(app->size)
                   ->OnClick(MkFunc1(&SetChecked, app))
                   ->IntoEl());
    col->Child(component::Checkbox::New(a, StrL("disabled"))
                   ->Label(StrL("Disabled"))
                   ->Disabled(true)
                   ->WithSize(app->size)
                   ->IntoEl());
    StorySectionAdd(sec, col);
    page->Child(sec);
    return page;
}

void CheckboxClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryCheckbox, CheckboxRender, CheckboxClick);
