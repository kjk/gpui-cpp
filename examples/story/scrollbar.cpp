#include "Story.h"

struct ScrollbarStory {
    static El* Render(ScrollbarStory* self, Ctx* cx);
};

El* ScrollbarStory::Render(ScrollbarStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(16)->W(kFill);
    El* sec = StorySection(cx, "Default",
                           "A scrollbar that allows users to scroll content.");
    El* box = Div(a)
                  ->W(288)
                  ->H(192)
                  ->Border(1, cx->theme().border)
                  ->ClipY()
                  ->ScrollY(0);
    El* list = Div(a)->FlexCol();
    for (int i = 1; i <= 40; i++) {
        list->Child(
            Div(a)
                ->H(28)
                ->PadX(8)
                ->ItemsCenter()
                ->JustifyBetween()
                ->BorderB(1, cx->theme().border)
                ->Child(StoryTxt(cx, StoryFmt(cx, "Activity %d", i), 12,
                                 cx->theme().foreground))
                ->Child(StoryTxt(
                    cx, i % 3 == 0 ? StrL("Completed") : StrL("Pending"), 12,
                    cx->theme().mutedFg)));
    }
    box->Child(list);
    StorySectionAdd(sec, box);
    page->Child(sec);
    return page;
}

STORY_PAGE(StoryScrollbar, ScrollbarStory);
