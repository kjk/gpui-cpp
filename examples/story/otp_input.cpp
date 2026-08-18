#include "Story.h"

struct OtpInputStory {
    // The Options dropdown flips masking on every input at once.
    bool masked = true;
    StoryToolbarState toolbar;

    static El* Render(OtpInputStory* self, Ctx* cx);
};

enum {
    OtpOptMasked = ToolbarOptMultiple
};

static void OtpToolbarAct(OtpInputStory* self, Ctx* cx, const ClickEvent*,
                          intptr_t act) {
    if (act == OtpOptMasked) {
        self->masked = !self->masked;
    } else {
        StoryToolbarApply(&self->toolbar, nullptr, (int)act);
    }
    Notify(cx);
}

El* OtpInputStory::Render(OtpInputStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(20)->W(kFill);
    StoryToolbarOpt opts[1] = {{"Masked", self->masked, OtpOptMasked}};
    page->Child(
        StoryToolbarOptions(cx, self, opts, 1, Listen(cx, &OtpToolbarAct)));

    El* def = StorySection(cx, "Default",
                           "Six cells with masking and value updates.");
    StorySectionAdd(def, component::OtpInput::New(cx, "", 0)
                             ->Id(StrL("otp"))
                             ->Masked(self->masked)
                             ->WithSize(self->toolbar.size)
                             ->IntoEl());
    page->Child(def);

    El* group = StorySection(cx, "Grouping",
                             "Cells can be shown as one or several groups.");
    El* groupCol = Div(a)->FlexCol()->Gap(16)->ItemsCenter();
    groupCol->Child(component::OtpInput::New(cx, "123456", 6)
                        ->Id(StrL("otp-small"))
                        ->Groups(1)
                        ->Masked(self->masked)
                        ->WithSize(self->toolbar.size)
                        ->IntoEl());
    groupCol->Child(component::OtpInput::New(cx, "012345", 6)
                        ->Id(StrL("otp-large"))
                        ->Groups(3)
                        ->Masked(self->masked)
                        ->WithSize(self->toolbar.size)
                        ->IntoEl());
    StorySectionAdd(group, groupCol);
    page->Child(group);

    El* csz = StorySection(cx, "Custom size", "Custom cell dimensions.");
    StorySectionAdd(csz, component::OtpInput::New(cx, "654321", 4)
                             ->Id(StrL("otp-sized"))
                             ->Slots(4)
                             ->Groups(1)
                             ->Masked(self->masked)
                             ->CellSize(55)
                             ->IntoEl());
    page->Child(csz);

    El* dis = StorySection(cx, "Disabled", "Disabled input with a value.");
    StorySectionAdd(dis, component::OtpInput::New(cx, "123456", 6)
                             ->Id(StrL("otp-disabled"))
                             ->Masked(self->masked)
                             ->Disabled(true)
                             ->WithSize(self->toolbar.size)
                             ->IntoEl());
    page->Child(dis);
    return page;
}

STORY_PAGE(StoryOtpInput, OtpInputStory);
