#include "Story.h"

static void PickList(StoryApp* app, int i) {
    app->listSel = i;
}

El* ListRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec =
        StorySection(a, "Default", "A list of items that can be selected.");
    StorySectionAdd(sec, component::List::New(a)
                             ->Item(StrL("Inbox"))
                             ->Item(StrL("Drafts"))
                             ->Item(StrL("Sent"))
                             ->Item(StrL("Archive"))
                             ->Item(StrL("Spam"))
                             ->Item(StrL("Trash"))
                             ->Selected(app->listSel)
                             ->OnSelect(MkFunc1(&PickList, app))
                             ->IntoEl());
    page->Child(sec);
    return page;
}

void ListClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryList, ListRender, ListClick);
