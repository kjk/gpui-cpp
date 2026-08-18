#include "Story.h"

struct TreeStory {
    static El* Render(TreeStory* self, Ctx* cx);
};

El* TreeStory::Render(TreeStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(12)->W(kFill);
    El* sec = StorySection(cx, "File tree",
                           "A tree view component for hierarchical data.");
    StorySectionAdd(sec, component::Tree::New(cx)
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

STORY_PAGE(StoryTree, TreeStory);
