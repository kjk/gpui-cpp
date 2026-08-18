#include "Story.h"

struct PaginationStory {
    int page = 3;
    int pageMany = 12;
    StoryToolbarState toolbar;

    static El* Render(PaginationStory* self, Ctx* cx);
    static void Click(PaginationStory* self, Ctx* cx, int id);
};

static void SetPage(PaginationStory* self, int p) {
    self->page = p;
}
static void SetPageMany(PaginationStory* self, int p) {
    self->pageMany = p;
}

El* PaginationStory::Render(PaginationStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(cx, &self->toolbar));

    El* def = StorySection(cx, "Default", nullptr);
    StorySectionAdd(def, component::Pagination::New(cx, self->page, 10)
                             ->OnChange(MkFunc1(&SetPage, self))
                             ->IntoEl());
    page->Child(def);

    El* many = StorySection(
        cx, "Visible Pages",
        "Control how many page links remain visible in a larger result set.");
    StorySectionAdd(many, component::Pagination::New(cx, self->pageMany, 50)
                              ->OnChange(MkFunc1(&SetPageMany, self))
                              ->IntoEl());
    page->Child(many);

    El* compact = StorySection(cx, "Compact Style", nullptr);
    StorySectionAdd(compact, component::Pagination::New(cx, self->page, 10)
                                 ->OnChange(MkFunc1(&SetPage, self))
                                 ->IntoEl());
    page->Child(compact);

    El* dis = StorySection(cx, "Disabled", nullptr);
    StorySectionAdd(dis, component::Pagination::New(cx, 4, 10)->IntoEl());
    page->Child(dis);
    return page;
}

void PaginationStory::Click(PaginationStory* self, Ctx* cx, int id) {
    if (StoryToolbarClick(&self->toolbar, id)) {
        return;
    }
    (void)cx;
    (void)id;
}

STORY_PAGE(StoryPagination, PaginationStory);
