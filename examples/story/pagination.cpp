#include "Story.h"

static void SetPage(StoryApp* app, int p) {
    app->page = p;
}
static void SetPageMany(StoryApp* app, int p) {
    app->pageMany = p;
}

El* PaginationRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(cx, app));

    El* def = StorySection(cx, "Default", nullptr);
    StorySectionAdd(def, component::Pagination::New(cx, app->page, 10)
                             ->OnChange(MkFunc1(&SetPage, app))
                             ->IntoEl());
    page->Child(def);

    El* many = StorySection(
        cx, "Visible Pages",
        "Control how many page links remain visible in a larger result set.");
    StorySectionAdd(many, component::Pagination::New(cx, app->pageMany, 50)
                              ->OnChange(MkFunc1(&SetPageMany, app))
                              ->IntoEl());
    page->Child(many);

    El* compact = StorySection(cx, "Compact Style", nullptr);
    StorySectionAdd(compact, component::Pagination::New(cx, app->page, 10)
                                 ->OnChange(MkFunc1(&SetPage, app))
                                 ->IntoEl());
    page->Child(compact);

    El* dis = StorySection(cx, "Disabled", nullptr);
    StorySectionAdd(dis, component::Pagination::New(cx, 4, 10)->IntoEl());
    page->Child(dis);
    return page;
}

void PaginationClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryPagination, PaginationRender, PaginationClick);
