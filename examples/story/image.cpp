#include "Story.h"

struct ImageStory {
    static El* Render(ImageStory* self, Ctx* cx);
    static void Click(ImageStory* self, Ctx* cx, int id);
};

El* ImageStory::Render(ImageStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* ph = Div(a)
                 ->W(160)
                 ->H(100)
                 ->ItemsCenter()
                 ->JustifyCenter()
                 ->Bg(th.muted)
                 ->Radius(th.radius)
                 ->Child(StoryTxt(cx, StrL("No image"), 12, th.mutedFg));

    El* remote = StorySection(cx, "Remote SVG",
                              "Remote images are not fetched in this port.");
    StorySectionAdd(remote, ph);
    page->Child(remote);

    El* sec = StorySection(
        cx, "Fallback",
        "Image display with a placeholder when no asset is loaded.");
    StorySectionAdd(
        sec, Div(a)
                 ->W(160)
                 ->H(100)
                 ->ItemsCenter()
                 ->JustifyCenter()
                 ->Bg(th.muted)
                 ->Radius(th.radius)
                 ->Child(StoryTxt(cx, StrL("No image"), 12, th.mutedFg)));
    page->Child(sec);
    return page;
}

void ImageStory::Click(ImageStory* self, Ctx* cx, int id) {
    (void)cx;
    (void)id;
}

STORY_PAGE(StoryImage, ImageStory);
