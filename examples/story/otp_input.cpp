#include "Story.h"

El* OtpInputRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec =
        StorySection(cx, "Default", "A one-time password input component.");
    StorySectionAdd(sec, component::OtpInput::New(cx, app->otpBuf, app->otpLen)
                             ->IntoEl());
    page->Child(sec);

    El* group = StorySection(cx, "Grouping", nullptr);
    StorySectionAdd(group,
                    component::OtpInput::New(cx, app->otpBuf, app->otpLen)
                        ->IntoEl());
    page->Child(group);

    El* csz = StorySection(cx, "Custom size", nullptr);
    StorySectionAdd(csz, component::OtpInput::New(cx, app->otpBuf, app->otpLen)
                             ->IntoEl());
    page->Child(csz);

    El* dis = StorySection(cx, "Disabled", nullptr);
    StorySectionAdd(dis, component::OtpInput::New(cx, app->otpBuf, app->otpLen)
                             ->IntoEl());
    page->Child(dis);
    return page;
}

void OtpInputClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryOtpInput, OtpInputRender, OtpInputClick);
