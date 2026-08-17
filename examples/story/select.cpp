#include "Story.h"

static void ToggleSel(StoryApp* app) {
    app->selectOpen = !app->selectOpen;
}
static void PickSel(StoryApp* app, int i) {
    app->selectIx = i;
    app->selectOpen = false;
}

static component::Select* Framework(Ctx* cx, StoryApp* app, const char* id) {
    Arena* a = cx->a;
    return component::Select::New(cx, Str(id))
        ->Option(StrL("GPUI"))
        ->Option(StrL("React"))
        ->Option(StrL("SwiftUI"))
        ->Option(StrL("Vue"))
        ->Selected(app->selectIx)
        ->Open(app->selectOpen && app->selB == (int)id[0])
        ->OnToggle(MkFunc0(&ToggleSel, app))
        ->OnChange(MkFunc1(&PickSel, app));
}

El* SelectRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* search = StorySection(cx, "Search and clear", nullptr);
    StorySectionAdd(search, Framework(cx, app, "framework")->IntoEl());
    page->Child(search);

    El* width = StorySection(cx, "Menu width", nullptr);
    StorySectionAdd(width, Framework(cx, app, "width")->IntoEl());
    page->Child(width);

    El* dis = StorySection(cx, "Disabled", nullptr);
    StorySectionAdd(dis, Framework(cx, app, "disabled")->IntoEl());
    page->Child(dis);

    El* prefix = StorySection(cx, "Title prefix", nullptr);
    StorySectionAdd(prefix, Framework(cx, app, "prefix")->IntoEl());
    page->Child(prefix);

    El* empty = StorySection(cx, "Empty", nullptr);
    StorySectionAdd(empty, component::Select::New(cx, StrL("empty"))
                               ->Open(false)
                               ->IntoEl());
    page->Child(empty);
    return page;
}

void SelectClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StorySelect, SelectRender, SelectClick);
