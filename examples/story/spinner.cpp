#include "Story.h"

El* SpinnerRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(a, app));
    El* sec = StorySection(a, "Default", "A loading spinner.");
    El* row = Div(a)->FlexRow()->Gap(16)->ItemsCenter();
    row->Child(component::Spinner::New(a)->WithSize(app->size)->IntoEl());
    row->Child(component::Spinner::New(a)->WithSize(UiSize::Small)->IntoEl());
    row->Child(component::Spinner::New(a)
                   ->WithSize(UiSize::Large)
                   ->Color(ThemeNow().primary)
                   ->IntoEl());
    StorySectionAdd(sec, row);
    page->Child(sec);
    return page;
}

void SpinnerClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StorySpinner, SpinnerRender, SpinnerClick);
