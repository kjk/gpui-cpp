#include "Story.h"

struct ImageStory {
    static El* Render(ImageStory* self, Ctx* cx);
};

El* ImageStory::Render(ImageStory*, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill)->ItemsCenter();

    El* remote = StorySection(cx, "Remote SVG",
                              "Loads and renders an SVG from a remote URL.");
    StorySectionBody(remote)->W(480);
    // The frame is the same; the image inside it is not fetched in this port,
    // as in the Rust window when the URL is unreachable.
    El* frame = Div(a)
                    ->FlexRow()
                    ->W(kFill)
                    ->H(180)
                    ->ItemsCenter()
                    ->JustifyCenter()
                    ->Radius(th.radiusLg)
                    ->Border(1, th.border);
    StorySectionAdd(remote, frame);
    page->Child(remote);
    return page;
}

STORY_PAGE(StoryImage, ImageStory);
