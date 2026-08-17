#include "Story.h"

El* OtpInputRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default", "A one-time password input component.");
    StorySectionAdd(sec, component::OtpInput::New(a, app->otpBuf, app->otpLen)->IntoEl());
    page->Child(sec);
    return page;
}

void OtpInputClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryOtpInput, OtpInputRender, OtpInputClick);
