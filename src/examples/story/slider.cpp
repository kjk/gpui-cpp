#include "Story.h"

static void SetSlider(StoryApp* app, float v) {
    (void)app;
    (void)v;
}

El* SliderRender(StoryApp* app, Arena* a) {
    (void)app;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default", "An input where the user selects a value from within a given range.");
    El* col = Div(a)->FlexCol()->Gap(8)->W(280);
    col->Child(component::Slider::New(a)->Value(0.64f)->OnChange(MkFunc1(&SetSlider, app))->IntoEl());
    StorySectionAdd(sec, col);
    page->Child(sec);
    return page;
}

void SliderClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StorySlider, SliderRender, SliderClick);
