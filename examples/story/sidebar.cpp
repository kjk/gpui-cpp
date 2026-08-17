#include "Story.h"

El* SidebarRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    (void)app;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(
        cx, "Default",
        "A composable, themeable and customizable sidebar component.");
    StorySectionAdd(sec, component::Sidebar::New(cx)
                             ->Title(StrL("Workspace"))
                             ->Item(StrL("Inbox"))
                             ->Item(StrL("Calendar"))
                             ->Item(StrL("Settings"))
                             ->Item(StrL("Search"))
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
