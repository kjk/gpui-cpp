#include "Story.h"

El* OtpInputRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec =
        StorySection(a, "Default", "A one-time password input component.");
    StorySectionAdd(sec, component::OtpInput::New(a, app->otpBuf, app->otpLen)
                             ->IntoEl());
    page->Child(sec);

    El* group = StorySection(a, "Grouping", nullptr);
    StorySectionAdd(group, component::OtpInput::New(a, app->otpBuf, app->otpLen)
                               ->IntoEl());
    page->Child(group);

    El* csz = StorySection(a, "Custom size", nullptr);
    StorySectionAdd(csz, component::OtpInput::New(a, app->otpBuf, app->otpLen)
                             ->IntoEl());
    page->Child(csz);

    El* dis = StorySection(a, "Disabled", nullptr);
    StorySectionAdd(dis, component::OtpInput::New(a, app->otpBuf, app->otpLen)
                             ->IntoEl());
    page->Child(dis);
    return page;
}

void OtpInputClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryOtpInput, OtpInputRender, OtpInputClick);
