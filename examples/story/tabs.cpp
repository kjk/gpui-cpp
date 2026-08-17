#include "Story.h"

enum {
    ClickStoryTab = 2500
};

El* TabsRender(StoryApp* app, Arena* a) {
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec =
        StorySection(a, "Default", "A set of layered sections of content.");
    const char* names[] = {"Overview", "Activity", "Settings"};
    const char* bodies[] = {"Workspace overview", "Recent activity",
                            "Workspace settings"};
    El* bar = Tabs::New(a, StrL("story-tabs"))
                  ->FlexRow()
                  ->Gap(16)
                  ->BorderB(1, th.border)
                  ->W(400);
    for (int i = 0; i < 3; i++) {
        El* t =
            Tab::New(a, StoryFmt(a, "tab-%d", i), ClickStoryTab + i)
                ->H(32)
                ->ItemsCenter()
                ->Child(StoryTxt(a, Str(names[i]), 13,
                                 i == app->tab ? th.foreground : th.mutedFg));
        if (i == app->tab) {
            t->BorderB(2, th.foreground);
        }
        bar->Child(t);
    }
    El* box =
        Div(a)->FlexCol()->W(400)->Child(bar)->Child(Div(a)->Pad(12)->Child(
            StoryTxt(a, Str(bodies[app->tab]), 13, th.mutedFg)));
    StorySectionAdd(sec, box);
    page->Child(sec);
    return page;
}

void TabsClick(StoryApp* app, int id) {
    if (id >= ClickStoryTab && id < ClickStoryTab + 3) {
        app->tab = id - ClickStoryTab;
    }
}

STORY_PAGE(StoryTabs, TabsRender, TabsClick);
