#include "Story.h"

El* TextareaRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* def = StorySection(cx, "Textarea", nullptr);
    StorySectionAdd(def,
                    component::Textarea::New(cx, StrL("notes"), app->areaBuf)
                        ->IntoEl());
    page->Child(def);

    El* nowrap = StorySection(cx, "No Wrap", nullptr);
    StorySectionAdd(nowrap,
                    component::Textarea::New(cx, StrL("notes-nw"), app->areaBuf)
                        ->IntoEl());
    page->Child(nowrap);

    El* grow = StorySection(cx, "Auto Grow", nullptr);
    StorySectionAdd(
        grow, component::Textarea::New(cx, StrL("notes-grow"), app->areaBuf)
                  ->IntoEl());
    page->Child(grow);

    El* both = StorySection(cx, "Auto Grow with No Wrap", nullptr);
    StorySectionAdd(
        both, component::Textarea::New(cx, StrL("notes-both"), app->areaBuf)
                  ->IntoEl());
    page->Child(both);

    El* chat = StorySection(cx, "Submit on Enter (Chat)", nullptr);
    StorySectionAdd(chat,
                    component::Textarea::New(cx, StrL("chat"), app->areaBuf)
                        ->IntoEl());
    page->Child(chat);
    return page;
}

void TextareaClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryTextarea, TextareaRender, TextareaClick);
