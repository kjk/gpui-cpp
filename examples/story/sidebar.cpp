#include "Story.h"

El* SidebarRender(StoryApp* app, Arena* a) {
    (void)app;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default", "A composable, themeable and customizable sidebar component.");
    StorySectionAdd(sec, component::Sidebar::New(a)
                             ->Title(StrL("Workspace"))
                             ->Item(StrL("Inbox"))
                             ->Item(StrL("Calendar"))
                             ->Item(StrL("Settings"))
                             ->Selected(0)
                             ->IntoEl());
    page->Child(sec);
    return page;
}

void SidebarClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StorySidebar, SidebarRender, SidebarClick);
