#include "Story.h"

static void SetStep(StoryApp* app, int i) {
    app->stepper = i;
}

El* StepperRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(
        a, "Default",
        "A stepper component to display progress through a sequence of steps.");
    StorySectionAdd(sec, component::Stepper::New(a)
                             ->Step(StrL("Account"))
                             ->Step(StrL("Profile"))
                             ->Step(StrL("Review"))
                             ->Current(app->stepper)
                             ->OnChange(MkFunc1(&SetStep, app))
                             ->IntoEl());
    page->Child(sec);
    return page;
}

void StepperClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryStepper, StepperRender, StepperClick);
