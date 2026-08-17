#include "Story.h"

El* DescriptionListRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    (void)app;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(
        a, "Default", "A list of terms and their corresponding descriptions.");
    StorySectionAdd(sec, component::DescriptionList::New(a)
                             ->Item(StrL("Status"), StrL("Published"))
                             ->Item(StrL("License"), StrL("Apache-2.0"))
                             ->Item(StrL("Version"), StrL("0.5.0"))
                             ->Item(StrL("Owner"), StrL("Jason Lee"))
                             ->Item(StrL("Updated"), StrL("Today"))
                             ->IntoEl());
    page->Child(sec);
    return page;
}

void DescriptionListClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryDescriptionList, DescriptionListRender, DescriptionListClick);
