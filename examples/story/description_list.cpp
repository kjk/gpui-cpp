#include "Story.h"

struct DescriptionListStory {
    static El* Render(DescriptionListStory* self, Ctx* cx);
    static void Click(DescriptionListStory* self, Ctx* cx, int id);
};

El* DescriptionListStory::Render(DescriptionListStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(
        cx, "Default", "A list of terms and their corresponding descriptions.");
    StorySectionAdd(sec, component::DescriptionList::New(cx)
                             ->Item(StrL("Status"), StrL("Published"))
                             ->Item(StrL("License"), StrL("Apache-2.0"))
                             ->Item(StrL("Version"), StrL("0.5.0"))
                             ->Item(StrL("Owner"), StrL("Jason Lee"))
                             ->Item(StrL("Updated"), StrL("Today"))
                             ->IntoEl());
    page->Child(sec);
    return page;
}

void DescriptionListStory::Click(DescriptionListStory* self, Ctx* cx, int id) {
    (void)cx;
    (void)id;
}

STORY_PAGE(StoryDescriptionList, DescriptionListStory);
