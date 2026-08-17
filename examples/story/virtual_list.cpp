#include "Story.h"

El* VirtualListRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(cx, app));
    El* sec = StorySection(
        cx, "Default",
        "A virtualized list for efficiently rendering large lists.");
    El* list = Div(a)
                   ->FlexCol()
                   ->W(288)
                   ->H(192)
                   ->Border(1, ThemeNow().border)
                   ->ClipY();
    for (int i = 0; i < 24; i++) {
        list->Child(
            Div(a)
                ->H(32)
                ->PadX(8)
                ->ItemsCenter()
                ->JustifyBetween()
                ->BorderB(1, ThemeNow().border)
                ->Child(StoryTxt(cx, StoryFmt(cx, "Customer %06d", i + 1), 12,
                                 ThemeNow().foreground))
                ->Child(StoryTxt(cx, StoryFmt(cx, "ID-%06d", 100000 + i), 12,
                                 ThemeNow().mutedFg)));
    }
    StorySectionAdd(sec, list);
    page->Child(sec);
    return page;
}

void VirtualListClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryVirtualList, VirtualListRender, VirtualListClick);
