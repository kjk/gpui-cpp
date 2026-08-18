#include "Story.h"

struct ListStory {
    int listSel = 0;
    static El* Render(ListStory* self, Ctx* cx);
};

static void PickList(ListStory* self, Ctx* cx, const ClickEvent*, intptr_t i) {
    self->listSel = i;
}

El* ListStory::Render(ListStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec =
        StorySection(cx, "Default", "A list of items that can be selected.");
    StorySectionAdd(sec, component::List::New(cx)
                             ->Item(StrL("Inbox"))
                             ->Item(StrL("Drafts"))
                             ->Item(StrL("Sent"))
                             ->Item(StrL("Archive"))
                             ->Item(StrL("Spam"))
                             ->Item(StrL("Trash"))
                             ->Selected(self->listSel)
                             ->OnSelect(Listen(cx, &PickList))
                             ->IntoEl());
    page->Child(sec);
    return page;
}

STORY_PAGE(StoryList, ListStory);
