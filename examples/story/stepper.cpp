#include "Story.h"

static void SetStep(StoryApp* app, int i) {
    app->stepper = i;
}

El* StepperRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(a, app));

    El* h = StorySection(a, "Horizontal Stepper", nullptr);
    StorySectionAdd(h, component::Stepper::New(a)
                           ->Step(StrL("Step 1"))
                           ->Step(StrL("Step 2"))
                           ->Step(StrL("Step 3"))
                           ->Current(app->stepper)
                           ->OnChange(MkFunc1(&SetStep, app))
                           ->IntoEl());
    page->Child(h);

    El* ic = StorySection(a, "Icon Stepper", nullptr);
    StorySectionAdd(ic, component::Stepper::New(a)
                            ->Step(StrL("Order Details"))
                            ->Step(StrL("Shipping"))
                            ->Step(StrL("Preview"))
                            ->Step(StrL("Finish"))
                            ->Current(app->stepper)
                            ->OnChange(MkFunc1(&SetStep, app))
                            ->IntoEl());
    page->Child(ic);

    El* v = StorySection(a, "Vertical Stepper", nullptr);
    El* vCol = Div(a)->FlexCol()->Gap(16);
    const char* titles[] = {"Step 1", "Step 2", "Step 3", "Step 4"};
    const char* descs[] = {"Description for step 1.", "Description for step 2.",
                           "Description for step 3.",
                           "Description for step 4."};
    const Theme& th = ThemeNow();
    for (int i = 0; i < 4; i++) {
        bool on = i == app->stepper;
        bool done = i < app->stepper;
        El* row = Div(a)->FlexRow()->Gap(12)->ItemsCenter();
        El* dot = Div(a)
                      ->W(22)
                      ->H(22)
                      ->Radius(11)
                      ->ItemsCenter()
                      ->JustifyCenter()
                      ->Bg(on || done ? th.primary : th.secondary);
        dot->Child(TextEl(a, StoryFmt(a, "%d", i + 1))
                       ->Font(11)
                       ->Fg(on || done ? th.primaryFg : th.secondaryFg));
        El* text = Div(a)->FlexCol()->Gap(2);
        text->Child(StoryTxt(a, Str(titles[i]), 13, th.foreground));
        text->Child(StoryTxt(a, Str(descs[i]), 12, th.mutedFg));
        row->Child(dot);
        row->Child(text);
        vCol->Child(row);
    }
    StorySectionAdd(v, vCol);
    page->Child(v);

    El* tc = StorySection(a, "Text Center", nullptr);
    StorySectionAdd(tc, component::Stepper::New(a)
                            ->Step(StrL("Step 1"))
                            ->Step(StrL("Step 2"))
                            ->Step(StrL("Step 3"))
                            ->Current(app->stepper)
                            ->OnChange(MkFunc1(&SetStep, app))
                            ->IntoEl());
    page->Child(tc);
    return page;
}

void StepperClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryStepper, StepperRender, StepperClick);
