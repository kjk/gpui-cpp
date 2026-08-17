#include "Story.h"

El* IconRender(StoryApp* app, Arena* a) {
    (void)app;
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec =
        StorySection(a, "Lucide subset", "Icons used by the Windows port.");
    IconName names[] = {
        IconName::Inbox,  IconName::Settings,    IconName::Calendar,
        IconName::Folder, IconName::Search,      IconName::Info,
        IconName::Check,  IconName::Plus,        IconName::Minus,
        IconName::Copy,   IconName::ChevronDown, IconName::CircleCheck};
    El* row = Div(a)->FlexRow()->Gap(16)->ItemsCenter();
    for (int i = 0; i < 12; i++) {
        row->Child(Div(a)
                       ->W(36)
                       ->H(36)
                       ->ItemsCenter()
                       ->JustifyCenter()
                       ->Border(1, th.border)
                       ->Radius(6)
                       ->Child(IconEl(a, names[i], 18)->Fg(th.foreground)));
    }
    StorySectionAdd(sec, row);
    page->Child(sec);
    return page;
}

void IconClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryIcon, IconRender, IconClick);
