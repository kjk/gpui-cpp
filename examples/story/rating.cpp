#include "Story.h"

static void SetRating(StoryApp* app, int v) {
    app->rating = v;
}

El* RatingRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default", "A rating component that allows users to rate items.");
    StorySectionAdd(sec, component::Rating::New(a)->Value(app->rating)->Max(5)->OnChange(MkFunc1(&SetRating, app))->IntoEl());
    page->Child(sec);
    return page;
}

void RatingClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryRating, RatingRender, RatingClick);
