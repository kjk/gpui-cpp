#include "Story.h"

struct PaginationStory {
    int page = 3;
    int pageMany = 12;
    StoryToolbarState toolbar;

    static El* Render(PaginationStory* self, Ctx* cx);
    static void Click(PaginationStory* self, Ctx* cx, int id);
};

static void SetPage(PaginationStory* self, Ctx* cx, const ClickEvent*,
                    intptr_t p) {
    self->page = p;
}
static void SetPageMany(PaginationStory* self, Ctx* cx, const ClickEvent*,
                        intptr_t p) {
    self->pageMany = p;
}

El* PaginationStory::Render(PaginationStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(cx, self));

    El* def = StorySection(cx, "Default", nullptr);
    StorySectionAdd(def, component::Pagination::New(cx, self->page, 10)
                             ->OnChange(Listen(cx, &SetPage))
                             ->IntoEl());
    page->Child(def);

    El* many = StorySection(
        cx, "Visible Pages",
        "Control how many page links remain visible in a larger result set.");
    StorySectionAdd(many, component::Pagination::New(cx, self->pageMany, 50)
                              ->OnChange(Listen(cx, &SetPageMany))
                              ->IntoEl());
    page->Child(many);

    El* compact = StorySection(cx, "Compact Style", nullptr);
    StorySectionAdd(compact, component::Pagination::New(cx, self->page, 10)
                                 ->OnChange(Listen(cx, &SetPage))
                                 ->IntoEl());
    page->Child(compact);

    El* dis = StorySection(cx, "Disabled", nullptr);
    StorySectionAdd(dis, component::Pagination::New(cx, 4, 10)->IntoEl());
    page->Child(dis);
    return page;
}

void PaginationStory::Click(PaginationStory* self, Ctx* cx, int id) {
    (void)cx;
    (void)id;
}

STORY_PAGE(StoryPagination, PaginationStory);
