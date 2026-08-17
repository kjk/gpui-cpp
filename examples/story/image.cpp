#include "Story.h"

El* ImageRender(StoryApp* app, Arena* a) {
    (void)app;
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Fallback", "Image display with a placeholder when no asset is loaded.");
    El* ph = Div(a)
                 ->W(160)
                 ->H(100)
                 ->ItemsCenter()
                 ->JustifyCenter()
                 ->Bg(th.muted)
                 ->Radius(th.radius)
                 ->Child(StoryTxt(a, StrL("No image"), 12, th.mutedFg));
    StorySectionAdd(sec, ph);
    page->Child(sec);
    return page;
}

void ImageClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryImage, ImageRender, ImageClick);
