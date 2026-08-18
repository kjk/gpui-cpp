#include "Story.h"

struct SidebarStory {
    static El* Render(SidebarStory* self, Ctx* cx);
    static void Click(SidebarStory* self, Ctx* cx, int id);
};

El* SidebarStory::Render(SidebarStory* self, Ctx* cx) {
    Arena* a = cx->a;
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

void SidebarStory::Click(SidebarStory* self, Ctx* cx, int id) {
    (void)cx;
    (void)id;
}

STORY_PAGE(StorySidebar, SidebarStory);
