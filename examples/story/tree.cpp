#include "Story.h"

El* TreeRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    (void)app;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "File tree",
                           "A tree view component for hierarchical data.");
    StorySectionAdd(sec, component::Tree::New(a)
                             ->Node(StrL("components"), -1, true, true)
                             ->Node(StrL("ui"), 0, true, true)
                             ->Node(StrL("button.rs"), 1, false, false)
                             ->Node(StrL("card.rs"), 1, false, false)
                             ->Node(StrL("dialog.rs"), 1, false, false)
                             ->Node(StrL("login_form.rs"), 0, false, false)
                             ->Node(StrL("main.rs"), -1, false, false)
                             ->IntoEl());
    page->Child(sec);
    return page;
}

void TreeClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryTree, TreeRender, TreeClick);
