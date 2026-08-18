#include "Story.h"

struct RatingStory {
    int rating = 3;
    StoryToolbarState toolbar;

    static El* Render(RatingStory* self, Ctx* cx);
};

static void SetRating(RatingStory* self, Ctx* cx, const ClickEvent*,
                      intptr_t v) {
    self->rating = v;
}

El* RatingStory::Render(RatingStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* page = Div(a)->FlexCol()->Gap(12)->W(kFill);
    page->Child(StoryToolbar(cx, self));

    El* def =
        StorySection(cx, "Default", "Select a value directly from the rating.");
    StorySectionAdd(def, component::Rating::New(cx)
                             ->Value(self->rating)
                             ->Max(5)
                             ->WithSize(self->toolbar.size)
                             ->OnChange(Listen(cx, &SetRating))
                             ->IntoEl());
    page->Child(def);

    El* dis = StorySection(cx, "Disabled", nullptr);
    StorySectionAdd(dis, component::Rating::New(cx)
                             ->Value(2)
                             ->Max(5)
                             ->Color(th.green)
                             ->Disabled(true)
                             ->WithSize(self->toolbar.size)
                             ->IntoEl());
    page->Child(dis);

    El* col = StorySection(cx, "Color", nullptr);
    StorySectionAdd(col, component::Rating::New(cx)
                             ->Value(self->rating)
                             ->Max(5)
                             ->Color(th.green)
                             ->WithSize(self->toolbar.size)
                             ->OnChange(Listen(cx, &SetRating))
                             ->IntoEl());
    page->Child(col);
    return page;
}

STORY_PAGE(StoryRating, RatingStory);
