#include "Story.h"

struct StepperStory {
    int stepper = 1;
    StoryToolbarState toolbar;

    static El* Render(StepperStory* self, Ctx* cx);
};

static void SetStep(StepperStory* self, Ctx* cx, const ClickEvent*,
                    intptr_t i) {
    self->stepper = i;
}

El* StepperStory::Render(StepperStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(12)->W(kFill);
    page->Child(StoryToolbar(cx, self));

    El* h = StorySection(cx, "Horizontal Stepper", nullptr);
    StorySectionAdd(h, component::Stepper::New(cx)
                           ->Step(StrL("Step 1"))
                           ->Step(StrL("Step 2"))
                           ->Step(StrL("Step 3"))
                           ->Current(self->stepper)
                           ->OnChange(Listen(cx, &SetStep))
                           ->IntoEl());
    page->Child(h);

    El* ic = StorySection(cx, "Icon Stepper", nullptr);
    StorySectionAdd(ic, component::Stepper::New(cx)
                            ->Step(StrL("Order Details"), IconName::Inbox)
                            ->Step(StrL("Shipping"), IconName::Bot)
                            ->Step(StrL("Preview"), IconName::Eye)
                            ->Step(StrL("Finish"), IconName::CircleCheck)
                            ->Current(self->stepper)
                            ->OnChange(Listen(cx, &SetStep))
                            ->IntoEl());
    page->Child(ic);

    El* v = StorySection(cx, "Vertical Stepper", nullptr);
    El* vCol = Div(a)->FlexCol()->Gap(0)->W(360);
    const char* titles[] = {"Step 1", "Step 2", "Step 3", "Step 4"};
    const char* descs[] = {"Description for step 1.", "Description for step 2.",
                           "Description for step 3.",
                           "Description for step 4."};
    const Theme& th = cx->theme();
    // A vertical stepper draws the connector down the marker column.
    static const IconName kStepIcons[4] = {IconName::Building2,
                                           IconName::Asterisk, IconName::Folder,
                                           IconName::CircleCheck};
    for (int i = 0; i < 4; i++) {
        bool on = i == self->stepper;
        bool done = i < self->stepper;
        El* row = Div(a)->FlexRow()->Gap(12)->ItemsStart();
        El* rail = Div(a)->FlexCol()->ItemsCenter()->W(24)->Shrink0();
        El* dot = Div(a)
                      ->W(24)
                      ->H(24)
                      ->Shrink0()
                      ->Radius(12)
                      ->ItemsCenter()
                      ->JustifyCenter()
                      ->Bg(on || done ? th.primary : th.secondary);
        dot->Child(IconEl(a, kStepIcons[i], 14)
                       ->Fg(on || done ? th.primaryFg : th.mutedFg));
        rail->Child(dot);
        if (i < 3) {
            rail->Child(Div(a)->W(1)->H(28)->Bg(done ? th.primary : th.border));
        }
        El* text = Div(a)->FlexCol()->Gap(2);
        text->Child(
            StoryTxt(cx, Str(titles[i]), 14, on ? th.foreground : th.mutedFg));
        text->Child(StoryTxt(cx, Str(descs[i]), 14, th.mutedFg));
        row->Child(rail);
        row->Child(text);
        vCol->Child(row);
    }
    StorySectionAdd(v, vCol);
    page->Child(v);

    El* tc = StorySection(cx, "Text Center", nullptr);
    StorySectionAdd(tc, component::Stepper::New(cx)
                            ->Step(StrL("Step 1"))
                            ->Step(StrL("Step 2"))
                            ->Step(StrL("Step 3"))
                            ->Current(self->stepper)
                            ->OnChange(Listen(cx, &SetStep))
                            ->IntoEl());
    page->Child(tc);
    return page;
}

STORY_PAGE(StoryStepper, StepperStory);
