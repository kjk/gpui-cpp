#include "Story.h"

static void SetPage(StoryApp* app, int p) {
    app->page = p;
}

El* PaginationRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default", "Pagination with page navigation.");
    StorySectionAdd(sec, component::Pagination::New(a, app->page, 8)
                             ->OnChange(MkFunc1(&SetPage, app))
                             ->IntoEl());
    page->Child(sec);
    return page;
}

void PaginationClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryPagination, PaginationRender, PaginationClick);
