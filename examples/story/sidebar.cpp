#include "Story.h"

struct SidebarStory {
    static El* Render(SidebarStory* self, Ctx* cx);
};

El* SidebarStory::Render(SidebarStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(16)->W(kFill);
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

STORY_PAGE(StorySidebar, SidebarStory);
