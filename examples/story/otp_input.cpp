#include "Story.h"

struct OtpInputStory {
    char otpBuf[8] = "12";
    int otpLen = 2;
    static El* Render(OtpInputStory* self, Ctx* cx);
};

El* OtpInputStory::Render(OtpInputStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(20)->W(kFill);
    El* sec =
        StorySection(cx, "Default", "A one-time password input component.");
    StorySectionAdd(sec,
                    component::OtpInput::New(cx, self->otpBuf, self->otpLen)
                        ->IntoEl());
    page->Child(sec);

    El* group = StorySection(cx, "Grouping", nullptr);
    StorySectionAdd(group,
                    component::OtpInput::New(cx, self->otpBuf, self->otpLen)
                        ->IntoEl());
    page->Child(group);

    El* csz = StorySection(cx, "Custom size", nullptr);
    StorySectionAdd(csz,
                    component::OtpInput::New(cx, self->otpBuf, self->otpLen)
                        ->IntoEl());
    page->Child(csz);

    El* dis = StorySection(cx, "Disabled", nullptr);
    StorySectionAdd(dis,
                    component::OtpInput::New(cx, self->otpBuf, self->otpLen)
                        ->IntoEl());
    page->Child(dis);
    return page;
}

STORY_PAGE(StoryOtpInput, OtpInputStory);
