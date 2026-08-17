#include "Story.h"

static void ToggleCombo(StoryApp* app) {
    app->comboOpen = !app->comboOpen;
}
static void PickCombo(StoryApp* app, int i) {
    app->selectIx = i;
    app->comboOpen = false;
}

El* ComboboxRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default", "Autocomplete input and command palette with a list of suggestions.");
    const char* opts[] = {"GPUI", "React", "SwiftUI", "Vue"};
    component::Combobox* cb = component::Combobox::New(a, StrL("frameworks"))
                                  ->Selected(Str(opts[app->selectIx]))
                                  ->Open(app->comboOpen)
                                  ->Query(&app->search)
                                  ->OnToggle(MkFunc0(&ToggleCombo, app))
                                  ->OnChange(MkFunc1(&PickCombo, app));
    cb->Option(StrL("GPUI"))->Option(StrL("React"))->Option(StrL("SwiftUI"))->Option(StrL("Vue"));
    StorySectionAdd(sec, cb->IntoEl());
    page->Child(sec);
    return page;
}

void ComboboxClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryCombobox, ComboboxRender, ComboboxClick);
