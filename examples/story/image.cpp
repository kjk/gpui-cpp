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
    // `img("https://.../sdk.svg").h_24()` inside the frame. gpui/image.h
    // fetches it with sys/http.h and svg.cpp turns it into draw ops, so this
    // is the same picture the Rust window shows — and the same empty frame
    // when the URL is unreachable.
    El* frame = Div(a)
                    ->FlexRow()
                    ->W(kFill)
                    ->H(180)
                    ->ItemsCenter()
                    ->JustifyCenter()
                    ->Radius(th.radiusLg)
                    ->Border(1, th.border);
    frame->Child(ImageEl(a, StrL("https://pub.lbkrs.com/files/202503/"
                                 "vEnnmgUM6bo362ya/sdk.svg"))
                     ->H(96));
    StorySectionAdd(remote, frame);
    page->Child(remote);
    return page;
}

STORY_PAGE(StoryImage, ImageStory);
