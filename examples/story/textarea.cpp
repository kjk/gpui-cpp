#include "Story.h"

El* TextareaRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default", "Displays a form textarea.");
    StorySectionAdd(sec,
                    component::Textarea::New(a, StrL("notes"), app->areaBuf)
                        ->IntoEl());
    page->Child(sec);
    return page;
}

void TextareaClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryTextarea, TextareaRender, TextareaClick);
