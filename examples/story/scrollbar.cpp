#include "Story.h"

El* ScrollbarRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    (void)app;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default",
                           "A scrollbar that allows users to scroll content.");
    El* box = Div(a)
                  ->W(288)
                  ->H(192)
                  ->Border(1, ThemeNow().border)
                  ->ClipY()
                  ->ScrollY(0);
    El* list = Div(a)->FlexCol();
    for (int i = 1; i <= 40; i++) {
        list->Child(Div(a)
                        ->H(28)
                        ->PadX(8)
                        ->ItemsCenter()
                        ->JustifyBetween()
                        ->BorderB(1, ThemeNow().border)
                        ->Child(StoryTxt(a, StoryFmt(a, "Activity %d", i), 12,
                                         ThemeNow().foreground))
                        ->Child(StoryTxt(
                            a, i % 3 == 0 ? StrL("Completed") : StrL("Pending"),
                            12, ThemeNow().mutedFg)));
    }
    box->Child(list);
    StorySectionAdd(sec, box);
    page->Child(sec);
    return page;
}

void ScrollbarClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryScrollbar, ScrollbarRender, ScrollbarClick);
