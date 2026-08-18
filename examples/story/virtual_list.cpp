#include "Story.h"

struct VirtualListStory {
    StoryToolbarState toolbar;

    static El* Render(VirtualListStory* self, Ctx* cx);
    static void Click(VirtualListStory* self, Ctx* cx, int id);
};

El* VirtualListStory::Render(VirtualListStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(cx, &self->toolbar));
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

void VirtualListStory::Click(VirtualListStory* self, Ctx* cx, int id) {
    if (StoryToolbarClick(&self->toolbar, id)) {
        return;
    }
    (void)cx;
    (void)id;
}

STORY_PAGE(StoryVirtualList, VirtualListStory);
