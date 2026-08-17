#include "Story.h"

El* BreadcrumbRender(StoryApp* app, Arena* a) {
    (void)app;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default", "Displays the path to the current resource.");
    StorySectionAdd(sec, component::Breadcrumb::New(a)
                             ->Item(StrL("Home"))
                             ->Item(StrL("Components"))
                             ->Item(StrL("Breadcrumb"))
                             ->IntoEl());
    page->Child(sec);
    return page;
}

void BreadcrumbClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryBreadcrumb, BreadcrumbRender, BreadcrumbClick);
