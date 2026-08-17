#include "Story.h"

static void SetRating(StoryApp* app, int v) {
    app->rating = v;
}

El* RatingRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(a, app));

    El* def =
        StorySection(a, "Default", "Select a value directly from the rating.");
    StorySectionAdd(def, component::Rating::New(a)
                             ->Value(app->rating)
                             ->Max(5)
                             ->WithSize(app->size)
                             ->OnChange(MkFunc1(&SetRating, app))
                             ->IntoEl());
    page->Child(def);

    El* dis = StorySection(a, "Disabled", nullptr);
    StorySectionAdd(dis, component::Rating::New(a)
                             ->Value(2)
                             ->Max(5)
                             ->Color(th.green)
                             ->Disabled(true)
                             ->WithSize(app->size)
                             ->IntoEl());
    page->Child(dis);

    El* col = StorySection(a, "Color", nullptr);
    StorySectionAdd(col, component::Rating::New(a)
                             ->Value(app->rating)
                             ->Max(5)
                             ->Color(th.green)
                             ->WithSize(app->size)
                             ->OnChange(MkFunc1(&SetRating, app))
                             ->IntoEl());
    page->Child(col);
    return page;
}

void RatingClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryRating, RatingRender, RatingClick);
