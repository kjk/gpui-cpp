#include "Story.h"

struct VirtualListStory {
    StoryToolbarState toolbar;

    static El* Render(VirtualListStory* self, Ctx* cx);
};

El* VirtualListStory::Render(VirtualListStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(8)->W(kFill);
    page->Child(StoryToolbar(cx, self));
    El* sec = StorySection(
        cx, "Default",
        "A virtualized list for efficiently rendering large lists.");
    El* list = Div(a)
                   ->FlexCol()
                   ->W(288)
                   ->H(192)
                   ->Border(1, cx->theme().border)
                   ->ClipY();
    for (int i = 0; i < 24; i++) {
        list->Child(
            Div(a)
                ->H(32)
                ->PadX(8)
                ->ItemsCenter()
                ->JustifyBetween()
                ->BorderB(1, cx->theme().border)
                ->Child(StoryTxt(cx, StoryFmt(cx, "Customer %06d", i + 1), 12,
                                 cx->theme().foreground))
                ->Child(StoryTxt(cx, StoryFmt(cx, "ID-%06d", 100000 + i), 12,
                                 cx->theme().mutedFg)));
    }
    StorySectionAdd(sec, list);
    page->Child(sec);
    return page;
}

STORY_PAGE(StoryVirtualList, VirtualListStory);
