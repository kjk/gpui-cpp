#include "Story.h"

El* MenuRender(StoryApp* app, Arena* a) {
    (void)app;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default", "A menu of actions.");
    StorySectionAdd(sec, component::Menu::New(a)
                             ->Item(StrL("New File"))
                             ->Item(StrL("Open…"))
                             ->Item(StrL("Save"))
                             ->Item(StrL("Quit"))
                             ->IntoEl());
    page->Child(sec);
    return page;
}

void MenuClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryMenu, MenuRender, MenuClick);
