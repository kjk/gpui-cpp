#include "Story.h"

static void ToggleSel(StoryApp* app) {
    app->selectOpen = !app->selectOpen;
}
static void PickSel(StoryApp* app, int i) {
    app->selectIx = i;
    app->selectOpen = false;
}

static component::Select* Framework(Arena* a, StoryApp* app, const char* id) {
    return component::Select::New(a, Str(id))
        ->Option(StrL("GPUI"))
        ->Option(StrL("React"))
        ->Option(StrL("SwiftUI"))
        ->Option(StrL("Vue"))
        ->Selected(app->selectIx)
        ->Open(app->selectOpen && app->selB == (int)id[0])
        ->OnToggle(MkFunc0(&ToggleSel, app))
        ->OnChange(MkFunc1(&PickSel, app));
}

El* SelectRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* search = StorySection(a, "Search and clear", nullptr);
    StorySectionAdd(search, Framework(a, app, "framework")->IntoEl());
    page->Child(search);

    El* width = StorySection(a, "Menu width", nullptr);
    StorySectionAdd(width, Framework(a, app, "width")->IntoEl());
    page->Child(width);

    El* dis = StorySection(a, "Disabled", nullptr);
    StorySectionAdd(dis, Framework(a, app, "disabled")->IntoEl());
    page->Child(dis);

    El* prefix = StorySection(a, "Title prefix", nullptr);
    StorySectionAdd(prefix, Framework(a, app, "prefix")->IntoEl());
    page->Child(prefix);

    El* empty = StorySection(a, "Empty", nullptr);
    StorySectionAdd(
        empty, component::Select::New(a, StrL("empty"))->Open(false)->IntoEl());
    page->Child(empty);
    return page;
}

void SelectClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StorySelect, SelectRender, SelectClick);
