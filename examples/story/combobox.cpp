#include "Story.h"

static void ToggleCombo(StoryApp* app) {
    app->comboOpen = !app->comboOpen;
}
static void PickCombo(StoryApp* app, int i) {
    app->selectIx = i;
    app->comboOpen = false;
}

static El* Combo(Ctx* cx, StoryApp* app, const char* id) {
    Arena* a = cx->a;
    const char* opts[] = {"GPUI", "React", "SwiftUI", "Vue"};
    component::Combobox* cb = component::Combobox::New(cx, Str(id))
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

El* ComboboxRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* def = StorySection(
        cx, "Default",
        "Autocomplete input and command palette with a list of suggestions.");
    StorySectionAdd(def, Combo(cx, app, "frameworks"));
    page->Child(def);

    El* multi = StorySection(cx, "Multiple", nullptr);
    StorySectionAdd(multi, Combo(cx, app, "multi"));
    page->Child(multi);

    El* groups = StorySection(cx, "Groups", nullptr);
    StorySectionAdd(groups, Combo(cx, app, "groups"));
    page->Child(groups);

    El* icons = StorySection(cx, "Icons", nullptr);
    StorySectionAdd(icons, Combo(cx, app, "icons"));
    page->Child(icons);

    El* footer = StorySection(cx, "Footer", nullptr);
    StorySectionAdd(footer, Combo(cx, app, "footer"));
    page->Child(footer);
    return page;
}

void ComboboxClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryCombobox, ComboboxRender, ComboboxClick);
