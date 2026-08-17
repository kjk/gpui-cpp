#include "Story.h"

static void ToggleCombo(StoryApp* app) {
    app->comboOpen = !app->comboOpen;
}
static void PickCombo(StoryApp* app, int i) {
    app->selectIx = i;
    app->comboOpen = false;
}

static El* Combo(Arena* a, StoryApp* app, const char* id) {
    const char* opts[] = {"GPUI", "React", "SwiftUI", "Vue"};
    component::Combobox* cb = component::Combobox::New(a, Str(id))
                                  ->Selected(Str(opts[app->selectIx]))
                                  ->Open(app->comboOpen)
                                  ->Query(&app->search)
                                  ->OnToggle(MkFunc0(&ToggleCombo, app))
                                  ->OnChange(MkFunc1(&PickCombo, app));
    cb->Option(StrL("GPUI"))
        ->Option(StrL("React"))
        ->Option(StrL("SwiftUI"))
        ->Option(StrL("Vue"));
    return cb->IntoEl();
}

El* ComboboxRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* def = StorySection(
        a, "Default",
        "Autocomplete input and command palette with a list of suggestions.");
    StorySectionAdd(def, Combo(a, app, "frameworks"));
    page->Child(def);

    El* multi = StorySection(a, "Multiple", nullptr);
    StorySectionAdd(multi, Combo(a, app, "multi"));
    page->Child(multi);

    El* groups = StorySection(a, "Groups", nullptr);
    StorySectionAdd(groups, Combo(a, app, "groups"));
    page->Child(groups);

    El* icons = StorySection(a, "Icons", nullptr);
    StorySectionAdd(icons, Combo(a, app, "icons"));
    page->Child(icons);

    El* footer = StorySection(a, "Footer", nullptr);
    StorySectionAdd(footer, Combo(a, app, "footer"));
    page->Child(footer);
    return page;
}

void ComboboxClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryCombobox, ComboboxRender, ComboboxClick);
