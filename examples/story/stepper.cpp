#include "Story.h"

struct StepperStory {
    int stepper = 1;
    StoryToolbarState toolbar;

    static El* Render(StepperStory* self, Ctx* cx);
    static void Click(StepperStory* self, Ctx* cx, int id);
};

static void SetStep(StepperStory* self, Ctx* cx, const ClickEvent*,
                    intptr_t i) {
    self->stepper = i;
}

El* StepperStory::Render(StepperStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(cx, &self->toolbar));

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
                            ->Step(StrL("Order Details"))
                            ->Step(StrL("Shipping"))
                            ->Step(StrL("Preview"))
                            ->Step(StrL("Finish"))
                            ->Current(self->stepper)
                            ->OnChange(Listen(cx, &SetStep))
                            ->IntoEl());
    page->Child(ic);

    El* v = StorySection(cx, "Vertical Stepper", nullptr);
    El* vCol = Div(a)->FlexCol()->Gap(16);
    const char* titles[] = {"Step 1", "Step 2", "Step 3", "Step 4"};
    const char* descs[] = {"Description for step 1.", "Description for step 2.",
                           "Description for step 3.",
                           "Description for step 4."};
    const Theme& th = ThemeNow();
    for (int i = 0; i < 4; i++) {
        bool on = i == self->stepper;
        bool done = i < self->stepper;
        El* row = Div(a)->FlexRow()->Gap(12)->ItemsCenter();
        El* dot = Div(a)
                      ->W(22)
                      ->H(22)
                      ->Radius(11)
                      ->ItemsCenter()
                      ->JustifyCenter()
                      ->Bg(on || done ? th.primary : th.secondary);
        dot->Child(TextEl(a, StoryFmt(cx, "%d", i + 1))
                       ->Font(11)
                       ->Fg(on || done ? th.primaryFg : th.secondaryFg));
        El* text = Div(a)->FlexCol()->Gap(2);
        text->Child(StoryTxt(cx, Str(titles[i]), 13, th.foreground));
        text->Child(StoryTxt(cx, Str(descs[i]), 12, th.mutedFg));
        row->Child(dot);
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

void StepperStory::Click(StepperStory* self, Ctx* cx, int id) {
    if (StoryToolbarClick(&self->toolbar, id)) {
        return;
    }
    (void)cx;
    (void)id;
}

STORY_PAGE(StoryStepper, StepperStory);
