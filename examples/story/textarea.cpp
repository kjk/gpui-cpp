#include "Story.h"

struct TextareaStory {
    char areaBuf[512] = "Build focused interfaces.";
    static El* Render(TextareaStory* self, Ctx* cx);
    static void Click(TextareaStory* self, Ctx* cx, int id);
};

El* TextareaStory::Render(TextareaStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* def = StorySection(cx, "Textarea", nullptr);
    StorySectionAdd(def,
                    component::Textarea::New(cx, StrL("notes"), self->areaBuf)
                        ->IntoEl());
    page->Child(def);

    El* nowrap = StorySection(cx, "No Wrap", nullptr);
    StorySectionAdd(
        nowrap, component::Textarea::New(cx, StrL("notes-nw"), self->areaBuf)
                    ->IntoEl());
    page->Child(nowrap);

    El* grow = StorySection(cx, "Auto Grow", nullptr);
    StorySectionAdd(
        grow, component::Textarea::New(cx, StrL("notes-grow"), self->areaBuf)
                  ->IntoEl());
    page->Child(grow);

    El* both = StorySection(cx, "Auto Grow with No Wrap", nullptr);
    StorySectionAdd(
        both, component::Textarea::New(cx, StrL("notes-both"), self->areaBuf)
                  ->IntoEl());
    page->Child(both);

    El* chat = StorySection(cx, "Submit on Enter (Chat)", nullptr);
    StorySectionAdd(chat,
                    component::Textarea::New(cx, StrL("chat"), self->areaBuf)
                        ->IntoEl());
    page->Child(chat);
    return page;
}

void TextareaStory::Click(TextareaStory* self, Ctx* cx, int id) {
    (void)cx;
    (void)id;
}

STORY_PAGE(StoryTextarea, TextareaStory);
