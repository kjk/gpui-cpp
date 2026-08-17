#include "Story.h"

static void SetSlider(StoryApp* app, float v) {
    (void)app;
    (void)v;
}

El* SliderRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    (void)app;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(
        a, "Default",
        "An input where the user selects a value from within a given range.");
    El* col = Div(a)->FlexCol()->Gap(8)->W(280);
    col->Child(component::Slider::New(a)
                   ->Value(0.64f)
                   ->OnChange(MkFunc1(&SetSlider, app))
                   ->IntoEl());
    StorySectionAdd(sec, col);
    page->Child(sec);

    El* range = StorySection(a, "Range", nullptr);
    El* rangeCol = Div(a)->FlexCol()->Gap(8)->W(280);
    rangeCol->Child(component::Slider::New(a)->Value(0.25f)->IntoEl());
    rangeCol->Child(component::Slider::New(a)->Value(0.75f)->IntoEl());
    StorySectionAdd(range, rangeCol);
    page->Child(range);

    El* rev = StorySection(a, "Reverse", nullptr);
    StorySectionAdd(rev, component::Slider::New(a)->Value(0.35f)->IntoEl());
    page->Child(rev);
    return page;
}

void SliderClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StorySlider, SliderRender, SliderClick);
