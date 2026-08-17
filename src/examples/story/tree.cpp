#include "Story.h"

El* TreeRender(StoryApp* app, Arena* a) {
    (void)app;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default", "A tree view component for hierarchical data.");
    StorySectionAdd(sec, component::Tree::New(a)
                             ->Node(StrL("src"), -1, true, true)
                             ->Node(StrL("lib.rs"), 0, false, false)
                             ->Node(StrL("examples"), -1, true, false)
                             ->Node(StrL("Cargo.toml"), -1, false, false)
                             ->IntoEl());
    page->Child(sec);
    return page;
}

void TreeClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryTree, TreeRender, TreeClick);
